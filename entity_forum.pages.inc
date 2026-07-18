<?php

/**
 * @file
 * Frontend page callbacks and forms for the Entity Forum module.
 */

/**
 * Adds the frontend stylesheet.
 *
 * Every frontend page callback needs the same stylesheet; this one-liner
 * keeps the path in a single place rather than repeated at each call site.
 */
function _entity_forum_add_css() {
  backdrop_add_css(backdrop_get_path('module', 'entity_forum') . '/css/entity_forum.css');
}

/**
 * Sets the forum breadcrumb: Home > Forums > ancestor chain > deepest.
 *
 * Each ancestor forum (and the deepest forum given) becomes a trail
 * link, so nested forums and topics show their full lineage. Pass the
 * deepest forum to include as a link — on a forum page that is the
 * forum's parent (the forum itself is the page title); on a topic page
 * it is the topic's own forum.
 *
 * @param int $deepest_forum_id
 *   The deepest forum to include as a trail link, or 0 for none.
 */
function _entity_forum_set_breadcrumb($deepest_forum_id) {
  $crumbs = array(
    l(t('Home'), '<front>'),
    l(t('Forums'), 'forums'),
  );
  if ($deepest_forum_id) {
    $titles = db_query('SELECT forum_id, title FROM {entity_forum_forum}')->fetchAllKeyed();
    $trail = entity_forum_forum_ancestors($deepest_forum_id);
    $trail[] = $deepest_forum_id;
    foreach ($trail as $fid) {
      if (isset($titles[$fid])) {
        $crumbs[] = l($titles[$fid], 'forum/' . $fid);
      }
    }
  }
  backdrop_set_breadcrumb($crumbs);
}

/**
 * Page callback: a single forum — its subforums and topic list.
 *
 * @param EntityForumForum $forum
 *   The forum entity from the URL.
 *
 * @return array
 *   Render array.
 */
function entity_forum_forum_page($forum) {
  $build = array();
  _entity_forum_add_css();

  // Breadcrumb: Home > Forums > ancestor chain (the forum itself is the
  // page title, so it is not repeated as a trail link).
  _entity_forum_set_breadcrumb((int) $forum->parent_id);

  if (!empty($forum->description)) {
    $build['description'] = array(
      '#markup' => '<div class="entity-forum-description">' . _entity_forum_format_description($forum) . '</div>',
    );
  }

  // Moderation panel for moderators, above the subforum and topic lists.
  if ($panel = _entity_forum_moderation_panel()) {
    $build['moderation'] = $panel;
  }

  // Sub-forums, if any: full rows with topic/post counts and last
  // activity (rolled up to include their own sub-forums), matching the
  // forums index rather than a plain bullet list.
  $subforums = db_query('SELECT forum_id, title, description, description_format, visibility FROM {entity_forum_forum} WHERE parent_id = :id AND status = 1 AND listed = 1 ORDER BY weight, title', array(':id' => $forum->id()))->fetchAll();
  $counts = entity_forum_forum_counts();
  $subforum_rows = array();
  foreach ($subforums as $subforum) {
    if ($subforum->visibility === 'private' && !user_access('view private entity forum') && !user_access('administer entity forum')) {
      continue;
    }
    $title = l($subforum->title, 'forum/' . $subforum->forum_id);
    if ($subforum->visibility === 'private') {
      $title .= ' <span class="entity-forum-private">' . t('(private)') . '</span>';
    }
    $cell = '<div class="entity-forum-title">' . $title;
    if (!empty($subforum->description)) {
      $cell .= '<div class="entity-forum-description">' . _entity_forum_format_description($subforum) . '</div>';
    }
    $cell .= '</div>';
    $sc = isset($counts[$subforum->forum_id]) ? $counts[$subforum->forum_id] : array('topic_count' => 0, 'post_count' => 0, 'last_active_time' => 0);
    $subforum_rows[] = array(
      $cell,
      $sc['topic_count'],
      $sc['post_count'],
      $sc['last_active_time'] ? format_date($sc['last_active_time'], 'short') : t('Never'),
    );
  }
  if ($subforum_rows) {
    $build['subforums'] = array(
      '#theme' => 'table',
      '#header' => array(t('Subforum'), t('Topics'), t('Posts'), t('Last activity')),
      '#rows' => $subforum_rows,
      '#attributes' => array('class' => array('entity-forum-subforums')),
    );
  }

  if (entity_forum_topic_create_access($forum)) {
    $build['new_topic'] = array(
      '#markup' => '<div class="entity-forum-actions">' . l(t('Post new topic'), 'forum/' . $forum->id() . '/new-topic') . '</div>',
    );
  }
  elseif (user_is_logged_in() && user_access('create entity forum topic')
    && entity_forum_user_posting_status() === ENTITY_FORUM_POSTING_SUSPENDED) {
    $build['suspended'] = array(
      '#markup' => '<div class="entity-forum-suspended">' . t('Your forum posting privileges are currently suspended.') . '</div>',
    );
  }

  // Topic list: a view display taking the forum ID as its argument, chosen on
  // the forum (default: the shipped entity_forum_topics view — sticky first,
  // then latest activity). Embedded so site builders can adjust the columns at
  // admin/structure/views. Access checks, breadcrumbs, subforums and the
  // "Post new topic" button stay in this page callback.
  //
  // A container forum takes no topics, so the list would always be empty —
  // showing "No topics in this forum yet" under a list of sub-forums reads
  // as a fault rather than a design.
  if (!entity_forum_forum_is_container($forum)) {
    $topic_list = entity_forum_forum_topic_list_view($forum);
    $build['topics'] = array(
      '#markup' => '<div class="entity-forum-topics">' . views_embed_view($topic_list['view'], $topic_list['display'], $forum->id()) . '</div>',
    );
  }

  // Let other modules alter the forum page (the topic list itself is the
  // entity_forum_topics view — alter that via the Views UI or views hooks).
  backdrop_alter('entity_forum_forum_page', $build, $forum);
  return $build;
}

/**
 * Page callback: a single topic with its replies and the reply form.
 *
 * @param EntityForumTopic $topic
 *   The topic entity from the URL.
 *
 * @return array
 *   Render array.
 */
function entity_forum_topic_page($topic) {
  $build = array(
    '#prefix' => '<div class="entity-forum-topic-page">',
    '#suffix' => '</div>',
  );
  _entity_forum_add_css();

  if (!$topic->status) {
    // Unpublished topics are readable by moderators/admins and by their
    // own author, so a member can review a topic that is not yet live.
    $is_author = $GLOBALS['user']->uid > 0 && (int) $topic->author_uid === (int) $GLOBALS['user']->uid;
    if (!$is_author && !entity_forum_user_is_moderator() && !user_access('administer entity forum topic')) {
      backdrop_access_denied();
      backdrop_exit();
    }
    backdrop_set_message(t('This topic is unpublished and not visible to other members.'), 'warning');
  }

  // Non-moderators are already blocked by entity_forum_topic_view_access();
  // tell moderators why this topic is off the public forum list.
  if ($topic->listing === ENTITY_FORUM_LISTING_ADMIN_ONLY) {
    backdrop_set_message(t('This topic is visible only to moderators.'), 'warning');
  }

  // Breadcrumb: Home > Forums > ancestor chain > the topic's forum.
  _entity_forum_set_breadcrumb((int) $topic->forum_id);

  // Node-attached topics are read on their node's page (the comments
  // replacement) — cross-link it for anyone landing on the topic URL.
  if (!empty($topic->node_id) && ($discussion_node = node_load($topic->node_id))) {
    $build['node_link'] = array(
      '#markup' => '<div class="entity-forum-node-link">' . t('This discussion belongs to !link.', array(
        '!link' => l($discussion_node->title, 'node/' . $discussion_node->nid),
      )) . '</div>',
      '#weight' => -20,
    );
  }

  // The opening post. Every sibling on this page carries an explicit
  // #weight: element_children() sorts through an UNSTABLE sort as soon as
  // any one of them has a #weight, so leaving some weightless lets them
  // reorder arbitrarily (see _entity_forum_topic_replies_build()).
  $topic_fields = _entity_forum_render_fields('entity_forum_topic', array($topic->id() => $topic));
  $highlight = _entity_forum_post_report_highlight($topic, 'topic', $topic->id());
  $build['topic'] = array(
    '#weight' => 0,
    '#prefix' => '<div class="entity-forum-topic-wrapper"><h2 class="entity-forum-topic-title">' . check_plain($topic->title) . '</h2>',
    '#suffix' => '</div>',
    '#markup' => _entity_forum_render_post(
      _entity_forum_post_body_html('entity_forum_topic', $topic),
      $topic->author_uid,
      $topic->created,
      'topic-' . $topic->id(),
      _entity_forum_post_links('topic', $topic, $topic),
      $highlight['prefix'],
      $topic->author_name,
      isset($topic_fields[$topic->id()]) ? $topic_fields[$topic->id()] : '',
      $highlight['classes'],
      'entity_forum_topic',
      $topic->bundle()
    ),
  );

  // Replies and the reply/report actions — shared with the node
  // discussion rendering (comments replacement).
  $build += _entity_forum_topic_replies_build($topic);

  // Let other modules alter the topic page (posts, reply form and all).
  backdrop_alter('entity_forum_topic_page', $build, $topic);
  return $build;
}

/**
 * Builds the shared reply section of a discussion.
 *
 * Everything below the opening post: the threaded replies with their
 * pager and the reply call to action (or the closed / suspended / log-in
 * notice). Used by the topic page and by the node discussion section
 * (entity_forum_node_discussion_build()).
 *
 * @param EntityForumTopic $topic
 *   The topic whose replies are rendered.
 *
 * @return array
 *   Render array fragment (keys: replies, pager, and one of closed /
 *   reply_cta / suspended / login).
 */
function _entity_forum_topic_replies_build($topic) {
  $build = array();

  // Replies, threaded Facebook-style: one visual level — every reply in
  // a chain nests under its thread root, indented once. The pager works
  // on thread roots (50 per page) so a thread is never split across
  // pages. The id/parent map for the whole topic is loaded first (cheap:
  // three small columns), threads are grouped in PHP, then only the
  // visible page's entities are fully loaded.
  $all = db_query('SELECT reply_id, reply_to_id, created FROM {entity_forum_reply} WHERE topic_id = :tid AND status = 1 ORDER BY created ASC, reply_id ASC', array(
    ':tid' => $topic->id(),
  ))->fetchAllAssoc('reply_id');

  $roots = array();
  $threads = array();
  foreach ($all as $reply_id => $row) {
    $root_id = _entity_forum_reply_thread_root($reply_id, $all);
    if ($root_id == $reply_id) {
      $roots[] = $reply_id;
      if (!isset($threads[$reply_id])) {
        $threads[$reply_id] = array();
      }
    }
    else {
      $threads[$root_id][] = $reply_id;
    }
  }

  $per_page = 50;
  $page = pager_default_initialize(count($roots), $per_page);
  $page_roots = array_slice($roots, $page * $per_page, $per_page);

  $visible_ids = array();
  foreach ($page_roots as $root_id) {
    $visible_ids[] = $root_id;
    foreach ($threads[$root_id] as $child_id) {
      $visible_ids[] = $child_id;
    }
  }

  // Attached-field display needs the full entities for this page.
  $reply_entities = $visible_ids ? entity_load('entity_forum_reply', $visible_ids) : array();
  $reply_fields = _entity_forum_render_fields('entity_forum_reply', $reply_entities);

  $items = '';
  foreach ($page_roots as $root_id) {
    foreach (array_merge(array($root_id), $threads[$root_id]) as $reply_id) {
      if (!isset($reply_entities[$reply_id])) {
        continue;
      }
      $reply = $reply_entities[$reply_id];
      $is_child = $reply_id != $root_id;
      // "In reply to" context only when the parent is not the thread
      // root the reply is already displayed under.
      $prefix = '';
      if ($is_child && $reply->reply_to_id && $reply->reply_to_id != $root_id) {
        $prefix = '<div class="entity-forum-reply-context">' . l(t('In reply to #@id', array('@id' => $reply->reply_to_id)), current_path(), array('fragment' => 'reply-' . $reply->reply_to_id)) . '</div>';
      }
      $highlight = _entity_forum_post_report_highlight($topic, 'reply', $reply_id);
      $post = _entity_forum_render_post(
        _entity_forum_post_body_html('entity_forum_reply', $reply),
        $reply->author_uid,
        $reply->created,
        'reply-' . $reply_id,
        _entity_forum_post_links('reply', $reply, $topic),
        $highlight['prefix'] . $prefix,
        $reply->author_name,
        isset($reply_fields[$reply_id]) ? $reply_fields[$reply_id] : '',
        $highlight['classes'],
        'entity_forum_reply',
        $reply->bundle()
      );
      $items .= $is_child ? '<div class="entity-forum-reply-child">' . $post . '</div>' : $post;
    }
  }
  // Every sibling carries an explicit #weight. element_children() sorts
  // through backdrop_sort() / array_multisort() — an UNSTABLE sort — as
  // soon as any one sibling has a #weight, which would otherwise scramble
  // the weightless ones and float the replies above the opening post.
  // Keeping them all weighted makes the order deterministic regardless of
  // what future elements are added here.
  $build['replies'] = array('#markup' => $items, '#weight' => 10);
  $build['pager'] = array('#theme' => 'pager', '#weight' => 20);

  // Reply call to action. Each post has its own "Reply" link; this closing
  // link replies to the topic itself.
  if ($topic->closed) {
    $build['closed'] = array(
      '#markup' => '<div class="entity-forum-closed">' . t('This topic is closed to new replies.') . '</div>',
      '#weight' => 30,
    );
  }
  elseif (entity_forum_reply_create_access($topic)) {
    $reply_path = 'topic/' . $topic->id() . '/reply';
    $build['reply_cta'] = array(
      '#markup' => '<div class="entity-forum-actions">' . l(t('Reply to this topic'), $reply_path, array(
        'attributes' => array('class' => array('entity-forum-reply-link', 'button')),
      )) . '</div>',
      '#weight' => 30,
    );
  }
  elseif (user_is_logged_in() && user_access('create entity forum reply')
    && entity_forum_user_posting_status() === ENTITY_FORUM_POSTING_SUSPENDED) {
    $build['suspended'] = array(
      '#markup' => '<div class="entity-forum-suspended">' . t('Your forum posting privileges are currently suspended.') . '</div>',
      '#weight' => 30,
    );
  }
  elseif (user_is_anonymous()) {
    $build['login'] = array(
      '#markup' => '<div class="entity-forum-login">' . t('<a href="@login">Log in</a> to reply.', array(
        '@login' => url('user/login', array('query' => backdrop_get_destination())),
      )) . '</div>',
      '#weight' => 30,
    );
  }

  return $build;
}

/**
 * Builds the discussion section rendered below a node.
 *
 * The comments replacement: heading with reply count, the topic's
 * opening post when it has one (manually attached topics — the
 * auto-created discussion topics have an empty body because the node
 * itself is the opening post), then the shared reply section.
 *
 * @param EntityForumTopic $topic
 *   The node's discussion topic.
 *
 * @return array
 *   Render array for hook_node_view().
 */
function entity_forum_node_discussion_build($topic) {
  _entity_forum_add_css();
  $build = array(
    '#prefix' => '<div class="entity-forum-node-discussion">',
    '#suffix' => '</div>',
    '#weight' => 100,
  );
  $count = (int) $topic->reply_count;
  // Explicit weights keep heading → opening post → replies ordered; see
  // _entity_forum_topic_replies_build() for why every sibling needs one.
  $build['heading'] = array(
    '#markup' => '<h2 class="entity-forum-discussion-heading">' . ($count
      ? format_plural($count, 'Discussion (1 reply)', 'Discussion (@count replies)')
      : t('Discussion')) . '</h2>',
    '#weight' => -20,
  );
  // A node-attached discussion topic has an empty body (the node is the
  // opening post), so its opening post is skipped; a normal topic shown as a
  // discussion still renders its body.
  $opening_body = _entity_forum_post_body_html('entity_forum_topic', $topic);
  if ($opening_body !== '') {
    $topic_fields = _entity_forum_render_fields('entity_forum_topic', array($topic->id() => $topic));
    $highlight = _entity_forum_post_report_highlight($topic, 'topic', $topic->id());
    $build['opening'] = array(
      '#weight' => 0,
      '#markup' => _entity_forum_render_post(
        $opening_body,
        $topic->author_uid,
        $topic->created,
        'topic-' . $topic->id(),
        _entity_forum_post_links('topic', $topic, $topic),
        $highlight['prefix'],
        $topic->author_name,
        isset($topic_fields[$topic->id()]) ? $topic_fields[$topic->id()] : '',
        $highlight['classes'],
        'entity_forum_topic',
        $topic->bundle()
      ),
    );
  }
  $build += _entity_forum_topic_replies_build($topic);
  return $build;
}

/**
 * Resolves the thread root a reply belongs to.
 *
 * Walks reply_to_id links up until a reply with no (published) parent is
 * reached — that reply is the thread root the whole chain displays
 * under. A visited guard tolerates corrupt cycles; a dangling or
 * unpublished parent makes the reply a root itself.
 *
 * @param int $reply_id
 *   The reply to resolve.
 * @param array $map
 *   All published replies of the topic keyed by reply_id, each row with
 *   a reply_to_id property.
 *
 * @return int
 *   The thread root reply_id ($reply_id itself when it is a root).
 */
function _entity_forum_reply_thread_root($reply_id, array $map) {
  $current = (int) $reply_id;
  $seen = array();
  while (!empty($map[$current]->reply_to_id)
    && isset($map[(int) $map[$current]->reply_to_id])
    && empty($seen[$current])) {
    $seen[$current] = TRUE;
    $current = (int) $map[$current]->reply_to_id;
  }
  return $current;
}

/**
 * Page callback: the standalone reply form (topic/N/reply).
 *
 * Where the per-post "Reply" links lead. Threading via ?reply_to=N.
 *
 * @param EntityForumTopic $topic
 *   The topic being replied to.
 *
 * @return array
 *   Render array with the reply form.
 */
function entity_forum_reply_page($topic) {
  _entity_forum_add_css();
  _entity_forum_set_breadcrumb((int) $topic->forum_id);
  backdrop_set_title(t('Reply to: @title', array('@title' => $topic->title)));
  return backdrop_get_form('entity_forum_reply_frontend_form', $topic);
}

/**
 * Page callback: the report form (forum-report/{topic|reply}/{id}).
 *
 * Where the per-post "Report" links lead. Access (logged in, target
 * exists and is visible to the reporter) is checked by
 * entity_forum_report_access().
 *
 * @param string $type
 *   'topic' or 'reply'.
 * @param int $id
 *   The reported entity ID.
 *
 * @return array
 *   Render array with the report form.
 */
function entity_forum_report_page($type, $id) {
  _entity_forum_add_css();
  $entity = entity_load("entity_forum_$type", $id);
  $topic = $type === 'topic' ? $entity : entity_load('entity_forum_topic', $entity->topic_id);
  _entity_forum_set_breadcrumb((int) $topic->forum_id);
  backdrop_set_title(t('Report content in: @title', array('@title' => $topic->title)));
  return backdrop_get_form('entity_forum_report_form', $type, $entity, $topic);
}

/**
 * Form builder: report a post to the moderators.
 *
 * Shows what is being reported (author + excerpt) above a reason textarea.
 *
 * @param string $type
 *   'topic' or 'reply'.
 * @param object $entity
 *   The reported topic or reply entity.
 * @param EntityForumTopic $topic
 *   The topic the post belongs to (the entity itself for topics).
 */
function entity_forum_report_form($form, &$form_state, $type, $entity, $topic) {
  $form_state['report_type'] = $type;
  $form_state['report_entity'] = $entity;
  $form_state['report_topic'] = $topic;

  $body = entity_forum_post_body_value($entity);
  $excerpt = truncate_utf8(strip_tags($body['value']), 160, TRUE, TRUE);
  $author = entity_forum_format_author($entity->author_uid, $entity->author_name);
  $form['heading'] = array(
    '#markup' => '<h3>' . t('Report to the moderators') . '</h3>',
  );
  $form['target'] = array(
    '#markup' => '<div class="entity-forum-report-target"><em>' . t('Reporting !author:', array('!author' => $author)) . '</em> ' . check_plain($excerpt) . '</div>',
  );
  $form['reason'] = array(
    '#type' => 'textarea',
    '#title' => t('Why are you reporting this?'),
    '#rows' => 4,
    '#required' => TRUE,
    '#description' => t('Only the moderators see your report.'),
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 10);
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Send report'),
  );
  return $form;
}

/**
 * Submit handler for entity_forum_report_form().
 *
 * Files the report (one open report per member per post — a duplicate just
 * re-confirms) and returns the member to the topic or node page.
 */
function entity_forum_report_form_submit($form, &$form_state) {
  $type = $form_state['report_type'];
  $entity = $form_state['report_entity'];
  $topic = $form_state['report_topic'];

  $report_id = entity_forum_report_create("entity_forum_$type", (int) $entity->id(), $form_state['values']['reason']);
  backdrop_set_message($report_id
    ? t('Thank you — your report has been sent to the moderators.')
    : t('You have already reported this post; the moderators will review it.'));

  $form_state['redirect'] = entity_forum_topic_discussion_path($topic);
}

/**
 * Builds the moderation panel shown above forum lists to moderators.
 *
 * A compact table styled like the subforum rows, with one row per
 * non-empty moderation queue (topics awaiting review, replies awaiting
 * review — reported content joins in a later step), each linking to the
 * moderation page. Non-moderators, and moderators with nothing waiting,
 * get nothing.
 *
 * @return array|null
 *   Render array for the panel, or NULL when there is nothing to show.
 */
function _entity_forum_moderation_panel() {
  if (!entity_forum_user_is_moderator()) {
    return NULL;
  }
  $counts = entity_forum_pending_counts();
  $rows = array();
  if ($counts['topics']) {
    $rows[] = array(
      '<div class="entity-forum-title">' . l(t('Topics awaiting review'), 'forums/moderation') . '</div>',
      $counts['topics'],
    );
  }
  if ($counts['replies']) {
    $rows[] = array(
      '<div class="entity-forum-title">' . l(t('Replies awaiting review'), 'forums/moderation') . '</div>',
      $counts['replies'],
    );
  }
  if ($counts['reports']) {
    $rows[] = array(
      '<div class="entity-forum-title">' . l(t('Reported content'), 'forums/moderation') . '</div>',
      $counts['reports'],
    );
  }
  if (!$rows) {
    return NULL;
  }
  return array(
    '#theme' => 'table',
    '#header' => array(t('Moderation'), t('Items')),
    '#rows' => $rows,
    '#attributes' => array('class' => array('entity-forum-subforums', 'entity-forum-moderation')),
  );
}

/**
 * Page callback: the moderation queue (forums/moderation).
 *
 * Lists topics and replies held for review with Approve / Keep hidden
 * actions (CSRF-token protected links; see
 * entity_forum_moderation_action()). Reply content is rendered in full —
 * held replies are unpublished, so this queue is the only place a
 * moderator can read them. Edit and delete links appear when the viewer
 * additionally holds the administer topic/reply permissions.
 *
 * @return array
 *   Render array.
 */
function entity_forum_moderation_page() {
  $build = array();
  _entity_forum_add_css();
  _entity_forum_set_breadcrumb(0);

  $forum_titles = db_query('SELECT forum_id, title FROM {entity_forum_forum}')->fetchAllKeyed();

  // Topics awaiting review.
  $topics = db_query('SELECT topic_id, title, forum_id, author_uid, author_name, created, pending_reason FROM {entity_forum_topic} WHERE pending = 1 ORDER BY created ASC')->fetchAll();
  $topic_rows = array();
  foreach ($topics as $topic) {
    $topic_rows[] = array(
      l($topic->title, 'topic/' . $topic->topic_id),
      isset($forum_titles[$topic->forum_id]) ? check_plain($forum_titles[$topic->forum_id]) : t('N/A'),
      entity_forum_format_author($topic->author_uid, $topic->author_name),
      format_date($topic->created, 'short'),
      entity_forum_pending_reason_label($topic->pending_reason),
      _entity_forum_moderation_actions('topic', $topic->topic_id),
    );
  }
  $build['topics_title'] = array('#markup' => '<h3>' . t('Topics awaiting review') . '</h3>');
  $build['topics'] = array(
    '#theme' => 'table',
    '#header' => array(t('Topic'), t('Forum'), t('Author'), t('Posted'), t('Held by'), t('Actions')),
    '#rows' => $topic_rows,
    '#empty' => t('No topics are waiting for review.'),
  );

  // Replies awaiting review, with their body rendered for reading. The body
  // is a field now, so load the entities (created ASC) and render it rather
  // than reading a column.
  $reply_ids = db_query('SELECT reply_id FROM {entity_forum_reply} WHERE pending = 1 ORDER BY created ASC')->fetchCol();
  $replies = $reply_ids ? entity_load('entity_forum_reply', $reply_ids) : array();
  $reply_rows = array();
  foreach ($reply_ids as $reply_id) {
    if (!isset($replies[$reply_id])) {
      continue;
    }
    $reply = $replies[$reply_id];
    $topic_title = db_query('SELECT title FROM {entity_forum_topic} WHERE topic_id = :id', array(':id' => $reply->topic_id))->fetchField();
    $reply_rows[] = array(
      '<div class="entity-forum-moderation-content">' . _entity_forum_post_body_html('entity_forum_reply', $reply) . '</div>',
      $topic_title !== FALSE ? l($topic_title, 'topic/' . $reply->topic_id) : t('N/A'),
      entity_forum_format_author($reply->author_uid, $reply->author_name),
      format_date($reply->created, 'short'),
      entity_forum_pending_reason_label($reply->pending_reason),
      _entity_forum_moderation_actions('reply', $reply->reply_id),
    );
  }
  $build['replies_title'] = array('#markup' => '<h3>' . t('Replies awaiting review') . '</h3>');
  $build['replies'] = array(
    '#theme' => 'table',
    '#header' => array(t('Reply'), t('Topic'), t('Author'), t('Posted'), t('Held by'), t('Actions')),
    '#rows' => $reply_rows,
    '#empty' => t('No replies are waiting for review.'),
  );

  // Unresolved content reports. The reported post renders as an excerpt
  // linking to it in place; the reporter's reason shows in full.
  $reports = db_query('SELECT report_id, entity_type, entity_id, reporter_uid, reporter_name, reason, created FROM {entity_forum_report} WHERE resolved = 0 ORDER BY created ASC')->fetchAll();
  $report_rows = array();
  foreach ($reports as $report) {
    $is_topic = $report->entity_type === 'entity_forum_topic';
    $target = entity_load($report->entity_type, $report->entity_id);
    if ($target) {
      $target_body = entity_forum_post_body_value($target);
      $excerpt = truncate_utf8(strip_tags($target_body['value']), 120, TRUE, TRUE);
      $target_cell = $is_topic
        ? l($target->title, 'topic/' . $target->id())
        : l($excerpt !== '' ? $excerpt : t('Reply #@id', array('@id' => $target->id())), 'topic/' . $target->topic_id, array('fragment' => 'reply-' . $target->id()));
      $author_cell = entity_forum_format_author($target->author_uid, $target->author_name);
    }
    else {
      // Cascades remove reports with their target; a miss here is a
      // transient race at worst.
      $target_cell = t('N/A (deleted)');
      $author_cell = '';
    }
    $report_rows[] = array(
      $target_cell,
      $author_cell,
      '<div class="entity-forum-moderation-content">' . check_plain((string) $report->reason) . '</div>',
      entity_forum_format_author($report->reporter_uid, $report->reporter_name),
      format_date($report->created, 'short'),
      _entity_forum_report_actions($report),
    );
  }
  $build['reports_title'] = array('#markup' => '<h3>' . t('Reported content') . '</h3>');
  $build['reports'] = array(
    '#theme' => 'table',
    '#header' => array(t('Reported post'), t('Post author'), t('Reason'), t('Reported by'), t('Reported'), t('Actions')),
    '#rows' => $report_rows,
    '#empty' => t('No unresolved reports.'),
  );

  return $build;
}

/**
 * Builds the action links for one report row in the moderation queue.
 *
 * Resolve is a token-protected callback link; Edit and Delete act on
 * the REPORTED POST (not the report) and appear when the viewer's
 * entity access allows — deleting the post removes its reports too.
 *
 * @param object $report
 *   A row from {entity_forum_report}.
 *
 * @return string
 *   Rendered links.
 */
function _entity_forum_report_actions($report) {
  $id = (int) $report->report_id;
  $links = array(
    'resolve' => array(
      'title' => t('Resolve'),
      'href' => "forums/moderation/resolve/report/$id",
      'query' => array('token' => backdrop_get_token("entity_forum_moderation:resolve:report:$id")),
    ),
  );
  $type = $report->entity_type === 'entity_forum_topic' ? 'topic' : 'reply';
  $access_callback = "entity_forum_{$type}_access";
  $target = entity_load($report->entity_type, $report->entity_id);
  if ($target && $access_callback('update', $target)) {
    $links['edit'] = array('title' => t('Edit post'), 'href' => "admin/content/entity-forum/$type/{$report->entity_id}/edit", 'query' => backdrop_get_destination());
  }
  if ($target && $access_callback('delete', $target)) {
    $links['delete'] = array('title' => t('Delete post'), 'href' => "admin/content/entity-forum/$type/{$report->entity_id}/delete", 'query' => backdrop_get_destination());
  }
  return theme('links', array(
    'links' => $links,
    'attributes' => array('class' => array('links', 'inline')),
  ));
}

/**
 * Builds the action links for one moderation queue row.
 *
 * Approve and Keep hidden are token-protected callback links available to
 * every moderator; Edit and Delete (the admin forms) are added when the
 * viewer's entity access allows them.
 *
 * @param string $type
 *   'topic' or 'reply'.
 * @param int $id
 *   The entity ID.
 *
 * @return string
 *   Rendered links.
 */
function _entity_forum_moderation_actions($type, $id) {
  $links = array(
    'approve' => array(
      'title' => t('Approve'),
      'href' => "forums/moderation/approve/$type/$id",
      'query' => array('token' => backdrop_get_token("entity_forum_moderation:approve:$type:$id")),
    ),
    'hide' => array(
      'title' => t('Keep hidden'),
      'href' => "forums/moderation/hide/$type/$id",
      'query' => array('token' => backdrop_get_token("entity_forum_moderation:hide:$type:$id")),
    ),
  );
  $access_callback = "entity_forum_{$type}_access";
  $entity = entity_load("entity_forum_$type", $id);
  if ($entity && $access_callback('update', $entity)) {
    $links['edit'] = array('title' => t('Edit'), 'href' => "admin/content/entity-forum/$type/$id/edit", 'query' => backdrop_get_destination());
  }
  if ($entity && $access_callback('delete', $entity)) {
    $links['delete'] = array('title' => t('Delete'), 'href' => "admin/content/entity-forum/$type/$id/delete", 'query' => backdrop_get_destination());
  }
  return theme('links', array(
    'links' => $links,
    'attributes' => array('class' => array('links', 'inline')),
  ));
}

/**
 * Page callback: applies a moderation action to a held post.
 *
 * Approve releases the hold — a topic returns to the normal listing, a
 * reply is published (statistics and counts refresh through the entity
 * hooks). Keep hidden clears the pending flag only, so the post leaves
 * the queue but stays invisible (admin_only topic / unpublished reply);
 * it remains reachable through the admin overviews. Links carry a CSRF
 * token seeded per action, checked here on top of the menu access check.
 *
 * Resolving a report ('resolve' + type 'report') marks the report row
 * resolved with the acting moderator and time; the reported post itself
 * is untouched — act on it via the Edit/Delete links if needed.
 *
 * @param string $action
 *   'approve', 'hide' or 'resolve'.
 * @param string $type
 *   'topic', 'reply' or 'report'.
 * @param int $id
 *   The entity or report ID.
 *
 * @return int|void
 *   A menu status code on failure; otherwise redirects to the queue.
 */
function entity_forum_moderation_action($action, $type, $id) {
  $valid = ($action === 'resolve' && $type === 'report')
    || (in_array($action, array('approve', 'hide'), TRUE) && in_array($type, array('topic', 'reply'), TRUE));
  if (!$valid) {
    return MENU_NOT_FOUND;
  }
  if (!isset($_GET['token']) || !backdrop_valid_token($_GET['token'], "entity_forum_moderation:$action:$type:$id")) {
    return MENU_ACCESS_DENIED;
  }

  if ($action === 'resolve') {
    $updated = db_update('entity_forum_report')
      ->fields(array(
        'resolved' => 1,
        'resolved_uid' => (int) $GLOBALS['user']->uid,
        'resolved_time' => REQUEST_TIME,
      ))
      ->condition('report_id', $id)
      ->condition('resolved', 0)
      ->execute();
    if (!$updated) {
      return MENU_NOT_FOUND;
    }
    backdrop_set_message(t('The report has been resolved.'));
    watchdog('entity_forum', 'Moderation: resolved report @id.', array('@id' => $id), WATCHDOG_NOTICE);
    backdrop_goto('forums/moderation');
    return;
  }

  $entity = entity_load("entity_forum_$type", $id);
  if (!$entity) {
    return MENU_NOT_FOUND;
  }

  if ($action === 'approve') {
    if ($type === 'topic') {
      $entity->listing = ENTITY_FORUM_LISTING_NORMAL;
    }
    else {
      $entity->status = 1;
    }
    $entity->pending = 0;
    $entity->save();
    backdrop_set_message($type === 'topic'
      ? t('The topic %title has been approved and is now listed.', array('%title' => $entity->title))
      : t('The reply has been approved and is now published.'));
    watchdog('entity_forum', 'Moderation: approved @type @id.', array('@type' => $type, '@id' => $id), WATCHDOG_NOTICE);
  }
  else {
    $entity->pending = 0;
    $entity->save();
    backdrop_set_message($type === 'topic'
      ? t('The topic %title stays hidden (visible to moderators only). It can still be managed from the admin Topics list.', array('%title' => $entity->title))
      : t('The reply stays hidden (unpublished). It can still be managed from the admin Replies list.'));
    watchdog('entity_forum', 'Moderation: kept @type @id hidden.', array('@type' => $type, '@id' => $id), WATCHDOG_NOTICE);
  }

  backdrop_goto('forums/moderation');
}

/**
 * Renders the attached Field API fields for a batch of forum entities.
 *
 * Used by the topic page to show any fields a site has added via the
 * structure UI (admin/structure/entity-forum) below each post's content.
 *
 * @param string $entity_type
 *   The entity type machine name.
 * @param array $entities
 *   Entities keyed by ID.
 *
 * @return array
 *   Rendered field markup keyed by entity ID; empty string when an
 *   entity has no visible field output.
 */
function _entity_forum_render_fields($entity_type, array $entities) {
  $rendered = array();
  if (!$entities) {
    return $rendered;
  }
  field_attach_prepare_view($entity_type, $entities, 'full');
  entity_prepare_view($entity_type, $entities);
  foreach ($entities as $id => $entity) {
    $view = field_attach_view($entity_type, $entity, 'full');
    // The body renders in the post's own content slot (see
    // _entity_forum_post_body_html()), so keep it out of this block or it
    // would appear twice.
    unset($view['entity_forum_body']);
    $html = trim(backdrop_render($view));
    $rendered[$id] = $html === '' ? '' : '<div class="entity-forum-fields">' . $html . '</div>';
  }
  return $rendered;
}

/**
 * Renders a forum post's body field to HTML for its content slot.
 *
 * The body of a topic or reply is the entity_forum_body field (one instance
 * per topic type). It is rendered here, on its own, so it can sit in the
 * post's .entity-forum-content slot above the avatar-adjacent fields, rather
 * than flowing in with them via _entity_forum_render_fields().
 *
 * @param string $entity_type
 *   'entity_forum_topic' or 'entity_forum_reply'.
 * @param object $entity
 *   The loaded post entity.
 *
 * @return string
 *   The rendered, sanitized body HTML, or '' when the bundle has no body
 *   instance or the body is empty (e.g. a node-attached discussion topic).
 */
function _entity_forum_post_body_html($entity_type, $entity) {
  if (!field_info_instance($entity_type, 'entity_forum_body', $entity->bundle())) {
    return '';
  }
  $element = field_view_field($entity_type, $entity, 'entity_forum_body', array('label' => 'hidden'));
  return trim(backdrop_render($element));
}

/**
 * Renders a single forum post (topic body or reply).
 *
 * @param string $body_html
 *   The already-rendered, sanitized body HTML (see
 *   _entity_forum_post_body_html()); '' for a post with no body.
 * @param int $uid
 *   Author user ID.
 * @param int $created
 *   Creation timestamp.
 * @param string $html_id
 *   Anchor ID for permalinks (e.g. "reply-12").
 * @param string $links
 *   Rendered edit/delete links, if the user has access.
 * @param string $prefix
 *   Extra markup rendered above the byline (e.g. reply-to context).
 * @param string $author_name
 *   Stored author_name snapshot, shown when the account is gone.
 * @param string $fields
 *   Rendered attached-field markup (see _entity_forum_render_fields()).
 * @param array $extra_classes
 *   Additional CSS classes for the post wrapper (e.g. the moderator-only
 *   reported-content highlight).
 *
 * @return string
 *   Sanitized post HTML.
 */
function _entity_forum_render_post($body_html, $uid, $created, $html_id, $links = '', $prefix = '', $author_name = '', $fields = '', array $extra_classes = array(), $entity_type = 'entity_forum_topic', $bundle = '') {
  // The markup lives in the themeable 'entity_forum_post' template so a site
  // theme can restyle the whole post shell; the avatar/byline/signature, the
  // wrapper classes and the per-kind/per-topic-type theme hook suggestions are
  // derived in template_preprocess_entity_forum_post(). Attachments arrive as
  // ordinary fields, so they render inside the template with every other field —
  // the module has no attachment section of its own.
  return theme('entity_forum_post', array(
    'body_html' => $body_html,
    'uid' => $uid,
    'created' => $created,
    'html_id' => $html_id,
    'links' => $links,
    'prefix' => $prefix,
    'author_name' => $author_name,
    'fields' => $fields,
    'extra_classes' => $extra_classes,
    'entity_type' => $entity_type,
    'bundle' => $bundle,
  ));
}

/**
 * Returns the moderator-only reported-content highlight for a post.
 *
 * When the viewer is a moderator and the post has open reports, the
 * post gets a badge (prefix markup linking to the moderation queue)
 * and a highlight class — so reported content is easy to spot inside
 * the thread itself. Everyone else gets nothing.
 *
 * @param EntityForumTopic $topic
 *   The topic being rendered.
 * @param string $type
 *   'topic' or 'reply'.
 * @param int $id
 *   The post's entity ID.
 *
 * @return array
 *   Array with 'prefix' (badge markup or '') and 'classes' (array).
 */
function _entity_forum_post_report_highlight($topic, $type, $id) {
  if (entity_forum_user_is_moderator()) {
    $open = entity_forum_topic_open_reports((int) $topic->id());
    if (!empty($open[$type][(int) $id])) {
      return array(
        'prefix' => entity_forum_reported_badge($open[$type][(int) $id]),
        'classes' => array('entity-forum-reported'),
      );
    }
  }
  return array('prefix' => '', 'classes' => array());
}

/**
 * Builds reply/edit/delete links for a post, if the current user has access.
 *
 * The "Reply" link targets the reply form page. Replying to the opening
 * post starts a new thread (no reply_to); replying to a reply threads
 * under it.
 *
 * @param string $type
 *   'topic' or 'reply'.
 * @param object $entity_or_row
 *   The entity (or a DB row with matching properties).
 * @param EntityForumTopic|null $topic
 *   The topic the post belongs to; enables the "Reply" link.
 *
 * @return string
 *   Rendered links, or an empty string.
 */
function _entity_forum_post_links($type, $entity_or_row, $topic = NULL) {
  $access_callback = "entity_forum_{$type}_access";
  $id = $type === 'topic' ? $entity_or_row->topic_id : $entity_or_row->reply_id;
  $links = array();
  if ($topic && entity_forum_reply_create_access($topic)) {
    $reply_path = 'topic/' . $topic->topic_id . '/reply';
    $options = array();
    if ($type === 'reply') {
      $options['query'] = array('reply_to' => $id);
    }
    $links['reply'] = array(
      'title' => t('Reply'),
      'href' => $reply_path,
      'query' => isset($options['query']) ? $options['query'] : array(),
    );
  }
  // Report: logged-in members flag someone else's post for moderator
  // attention.
  if (user_is_logged_in() && $entity_or_row->author_uid != $GLOBALS['user']->uid) {
    $report_path = "forum-report/$type/$id";
    $links['report'] = array(
      'title' => t('Report'),
      'href' => $report_path,
    );
  }
  // Close/reopen for the topic author ("mark as sold") in forums that
  // opted in via the author_close setting.
  if ($type === 'topic' && $topic && entity_forum_topic_author_close_access($topic)) {
    $links['toggle_closed'] = array(
      'title' => $topic->closed ? t('Reopen topic') : t('Close topic'),
      'href' => 'topic/' . $id . '/toggle-closed',
      'query' => array('token' => backdrop_get_token('entity_forum_author_close:' . $id)),
    );
  }
  if ($access_callback('update', $entity_or_row)) {
    $links['edit'] = array('title' => t('Edit'), 'href' => "admin/content/entity-forum/$type/$id/edit");
  }
  if ($access_callback('delete', $entity_or_row)) {
    $links['delete'] = array('title' => t('Delete'), 'href' => "admin/content/entity-forum/$type/$id/delete");
  }
  if (!$links) {
    return '';
  }
  return theme('links', array(
    'links' => $links,
    'attributes' => array('class' => array('links', 'inline')),
  ));
}

/**
 * Page callback: the topic author closes or reopens their own topic.
 *
 * The classified-ads "mark as sold" flow: offered (and access granted —
 * see entity_forum_topic_author_close_access()) only to the author, in
 * forums with the author_close setting. CSRF-protected by a token
 * seeded with the topic ID.
 *
 * @param EntityForumTopic $topic
 *   The topic to close or reopen.
 *
 * @return int|void
 *   MENU_ACCESS_DENIED on a bad token; otherwise redirects back to the
 *   topic (or its node page for node-attached discussions).
 */
function entity_forum_topic_toggle_closed($topic) {
  if (!isset($_GET['token']) || !backdrop_valid_token($_GET['token'], 'entity_forum_author_close:' . $topic->id())) {
    return MENU_ACCESS_DENIED;
  }
  $topic->closed = $topic->closed ? 0 : 1;
  $topic->save();
  backdrop_set_message($topic->closed
    ? t('Your topic %title is now closed to new replies.', array('%title' => $topic->title))
    : t('Your topic %title has been reopened.', array('%title' => $topic->title)));
  watchdog('entity_forum', 'Author @action topic @id.', array(
    '@action' => $topic->closed ? 'closed' : 'reopened',
    '@id' => $topic->id(),
  ), WATCHDOG_NOTICE);
  backdrop_goto(entity_forum_topic_discussion_path($topic));
}

/**
 * Form builder: post a new topic from within a forum.
 *
 * A lightweight version of the admin topic form: the forum is fixed and
 * moderation controls are hidden. The topic type comes from the forum, so
 * there is nothing to choose — posting looks the same as it did before types
 * existed.
 *
 * @param EntityForumForum $forum
 *   The forum the topic will be posted in.
 */
function entity_forum_topic_frontend_form($form, &$form_state, $forum) {
  $form_state['forum'] = $forum;
  if (!isset($form_state['topic'])) {
    // The type must be set before field_attach_form(): it is the bundle, so
    // it decides which widgets the form gets.
    $topic_type = entity_forum_forum_topic_type($forum);
    if (!$topic_type) {
      // A container forum. entity_forum_topic_create_access() already keeps
      // the route shut, so this is unreachable in normal use — but building
      // a bundle-less topic would throw EntityMalformedException, and a
      // blank form is a kinder failure than a fatal.
      $form['container'] = array(
        '#markup' => '<div class="entity-forum-closed">' . t('%forum does not take topics; post in one of its sub-forums instead.', array(
          '%forum' => $forum->label(),
        )) . '</div>',
      );
      return $form;
    }
    $form_state['topic'] = entity_create('entity_forum_topic', array(
      'forum_id' => (int) $forum->id(),
      'type' => $topic_type->type,
    ));
  }

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#maxlength' => 255,
    '#required' => TRUE,
  );

  // The body (entity_forum_body) and any other fields a site has attached to
  // this topic type. The body is a field now, so its widget and text format
  // are whatever the forum builder configured on the type.
  field_attach_form('entity_forum_topic', $form_state['topic'], $form, $form_state);
  entity_forum_apply_extra_field_weights($form, 'entity_forum_topic', $form_state['topic']->type);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Post topic'),
  );
  return $form;
}

/**
 * Validation for entity_forum_topic_frontend_form(): attached fields.
 */
function entity_forum_topic_frontend_form_validate($form, &$form_state) {
  field_attach_form_validate('entity_forum_topic', $form_state['topic'], $form, $form_state);
}

/**
 * Submit handler for entity_forum_topic_frontend_form().
 *
 * Topics can be held for moderation — by the member's 'review' posting
 * status or by another module via hook_entity_forum_post_hold_alter()
 * (see entity_forum_post_hold()): pending + admin_only listing, so the
 * existing visibility machinery hides them until a moderator approves.
 * Held authors are sent back to the forum page — their own topic page
 * would deny them access.
 */
function entity_forum_topic_frontend_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $topic = $form_state['topic'];
  $topic->title = $values['title'];
  // Move the field values (including the body) onto the entity BEFORE the
  // hold decision: hook_entity_forum_post_hold_alter() receives the entity,
  // and a content-aware rule (spam/link check) needs the body present.
  field_attach_submit('entity_forum_topic', $topic, $form, $form_state);
  $hold = entity_forum_post_hold($topic, 'entity_forum_topic');
  $held = !empty($hold['hold']);
  if ($held) {
    $topic->listing = ENTITY_FORUM_LISTING_ADMIN_ONLY;
    $topic->pending = 1;
    $topic->pending_reason = substr((string) $hold['reason'], 0, 64);
  }
  $topic->save();

  if ($held) {
    backdrop_set_message(t('Thank you — your topic %title has been received and will appear once a moderator has approved it.', array('%title' => $topic->title)));
    $form_state['redirect'] = 'forum/' . $topic->forum_id;
  }
  elseif (!$topic->status) {
    // Unpublished by something other than the moderation hold — another
    // module's presave, say. The author can still view their own topic
    // page, which repeats the notice.
    backdrop_set_message(t('Your topic %title has been saved but is not published yet; it is visible only to you and the moderators.', array('%title' => $topic->title)));
    $form_state['redirect'] = 'topic/' . $topic->id();
  }
  else {
    backdrop_set_message(t('Your topic %title has been posted.', array('%title' => $topic->title)));
    $form_state['redirect'] = 'topic/' . $topic->id();
  }
}

/**
 * Form builder: post a reply.
 *
 * One reply form, served as its own page from topic/N/reply. Supports
 * threading via a ?reply_to=N query parameter pointing at the reply being
 * answered. Any fields a site has attached to the reply type are always
 * present — there is no cut-down variant to keep in step with this one.
 *
 * @param EntityForumTopic $topic
 *   The topic being replied to.
 */
function entity_forum_reply_frontend_form($form, &$form_state, $topic) {
  $form_state['topic'] = $topic;

  $reply_to = 0;
  if (isset($_GET['reply_to']) && ctype_digit((string) $_GET['reply_to'])) {
    $parent_exists = db_query('SELECT 1 FROM {entity_forum_reply} WHERE reply_id = :id AND topic_id = :topic', array(
      ':id' => $_GET['reply_to'],
      ':topic' => $topic->id(),
    ))->fetchField();
    if ($parent_exists) {
      $reply_to = (int) $_GET['reply_to'];
    }
  }

  $form['heading'] = array(
    '#markup' => '<h3>' . ($reply_to ? t('Reply to #@id', array('@id' => $reply_to)) : t('Post a reply')) . '</h3>',
  );
  $form['reply_to_id'] = array(
    '#type' => 'value',
    '#value' => $reply_to,
  );

  if (!isset($form_state['reply'])) {
    // The type must be set here, not left to hook_entity_presave(): it is
    // the bundle, so field_attach_form() below needs it to know which
    // widgets this reply gets. Presave re-derives it from the topic anyway,
    // which keeps saves honest whatever built them.
    $form_state['reply'] = entity_create('entity_forum_reply', array(
      'topic_id' => (int) $topic->id(),
      'forum_id' => (int) $topic->forum_id,
      'type' => $topic->type,
    ));
  }

  // The body (entity_forum_body, labelled "Reply") and any other fields a
  // site has attached to this reply type — its widget and text format are
  // whatever the forum builder configured on the topic type.
  field_attach_form('entity_forum_reply', $form_state['reply'], $form, $form_state);
  entity_forum_apply_extra_field_weights($form, 'entity_forum_reply', $form_state['reply']->type);

  $form['actions'] = array('#type' => 'actions', '#weight' => 10);
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Post reply'),
  );
  return $form;
}

/**
 * Validation for entity_forum_reply_frontend_form(): attached fields.
 */
function entity_forum_reply_frontend_form_validate($form, &$form_state) {
  field_attach_form_validate('entity_forum_reply', $form_state['reply'], $form, $form_state);
}

/**
 * Submit handler for entity_forum_reply_frontend_form().
 */
function entity_forum_reply_frontend_form_submit($form, &$form_state) {
  $topic = $form_state['topic'];
  $values = $form_state['values'];

  $reply = $form_state['reply'];
  $reply->reply_to_id = (int) $values['reply_to_id'];
  // Move the field values (including the body) onto the entity BEFORE the
  // hold decision — see the topic submit for why.
  field_attach_submit('entity_forum_reply', $reply, $form, $form_state);
  // Replies can be held for moderation — by the member's 'review'
  // posting status or by another module via
  // hook_entity_forum_post_hold_alter(): pending + unpublished,
  // invisible everywhere (topic page, stats, counts) until a moderator
  // approves.
  $hold = entity_forum_post_hold($reply, 'entity_forum_reply');
  $held = !empty($hold['hold']);
  if ($held) {
    $reply->status = 0;
    $reply->pending = 1;
    $reply->pending_reason = substr((string) $hold['reason'], 0, 64);
  }
  $reply->save();

  backdrop_set_message($held
    ? t('Thank you — your reply has been received and will appear once a moderator has approved it.')
    : t('Your reply has been posted.'));

  // A held reply is not visible, so its #reply-N anchor would not exist
  // — redirect to the plain page instead. Node-attached discussions
  // return to the node page, normal topics to the topic page.
  $redirect_path = entity_forum_topic_discussion_path($topic);
  $redirect_options = $held ? array() : array('fragment' => 'reply-' . $reply->id());
  $form_state['redirect'] = array($redirect_path, $redirect_options);
}

/**
 * Page callback: a member's Forum tab on their account page.
 *
 * Shows the "Forum posting name" form followed by the account's topics
 * and replies (newest first, paged independently) with links into the
 * forum. Access is limited to the account owner and user administrators
 * by entity_forum_user_posts_access().
 *
 * @param object $account
 *   The account whose posting name and posts are shown.
 *
 * @return array
 *   Render array.
 */
function entity_forum_user_posts_page($account) {
  $build = array();
  _entity_forum_add_css();

  $build['posting_name_form'] = backdrop_get_form('entity_forum_posting_name_form', $account);
  $build['posting_name_form']['#weight'] = -10;

  // Tell the account owner when their posting is restricted; moderators
  // manage the status through the form above instead.
  if ($GLOBALS['user']->uid == $account->uid) {
    $posting_status = entity_forum_user_posting_status($account);
    if ($posting_status === ENTITY_FORUM_POSTING_REVIEW) {
      $build['posting_status_notice'] = array(
        '#markup' => '<div class="messages info">' . t('Your new forum posts are checked by a moderator before they appear.') . '</div>',
        '#weight' => -11,
      );
    }
    elseif ($posting_status === ENTITY_FORUM_POSTING_SUSPENDED) {
      $build['posting_status_notice'] = array(
        '#markup' => '<div class="messages warning">' . t('Your forum posting privileges are currently suspended.') . '</div>',
        '#weight' => -11,
      );
    }
  }

  $build['intro'] = array(
    '#markup' => '<p>' . t('Forum posts by %name.', array(
      '%name' => entity_forum_user_display_name($account),
    )) . '</p>',
  );

  // Topics.
  $topics = db_select('entity_forum_topic', 't')
    ->extend('PagerDefault')
    ->fields('t', array('topic_id', 'title', 'created', 'reply_count', 'status', 'pending'))
    ->condition('t.author_uid', $account->uid)
    ->orderBy('t.created', 'DESC')
    ->limit(25);
  $topics->element(0);
  $topic_rows = array();
  foreach ($topics->execute() as $topic) {
    $topic_rows[] = array(
      l($topic->title, 'topic/' . $topic->topic_id),
      format_date($topic->created, 'short'),
      $topic->reply_count,
      $topic->pending ? t('Awaiting review') : ($topic->status ? t('Published') : t('Unpublished')),
    );
  }
  $build['topics_title'] = array('#markup' => '<h3>' . t('Topics started') . '</h3>');
  $build['topics'] = array(
    '#theme' => 'table',
    '#header' => array(t('Topic'), t('Posted'), t('Replies'), t('Status')),
    '#rows' => $topic_rows,
    '#empty' => t('No topics.'),
  );
  $build['topics_pager'] = array('#theme' => 'pager', '#element' => 0);

  // Replies. The body is a field, so load this page's reply entities and read
  // the body from them for the excerpt link (the column is gone).
  $replies = db_select('entity_forum_reply', 'r')
    ->extend('PagerDefault')
    ->fields('r', array('reply_id', 'topic_id', 'created', 'status', 'pending'))
    ->condition('r.author_uid', $account->uid)
    ->orderBy('r.created', 'DESC')
    ->limit(25);
  $replies->element(1);
  $reply_page = $replies->execute()->fetchAll();
  $reply_ids = array();
  foreach ($reply_page as $reply) {
    $reply_ids[] = $reply->reply_id;
  }
  $reply_entities = $reply_ids ? entity_load('entity_forum_reply', $reply_ids) : array();
  $reply_rows = array();
  foreach ($reply_page as $reply) {
    $topic_title = db_query('SELECT title FROM {entity_forum_topic} WHERE topic_id = :id', array(':id' => $reply->topic_id))->fetchField();
    $body = isset($reply_entities[$reply->reply_id]) ? entity_forum_post_body_value($reply_entities[$reply->reply_id]) : array('value' => '');
    $excerpt = truncate_utf8(strip_tags($body['value']), 80, TRUE, TRUE);
    $reply_rows[] = array(
      l($excerpt !== '' ? $excerpt : t('Reply #@id', array('@id' => $reply->reply_id)), 'topic/' . $reply->topic_id, array('fragment' => 'reply-' . $reply->reply_id)),
      $topic_title !== FALSE ? check_plain($topic_title) : t('N/A'),
      format_date($reply->created, 'short'),
      $reply->pending ? t('Awaiting review') : ($reply->status ? t('Published') : t('Unpublished')),
    );
  }
  $build['replies_title'] = array('#markup' => '<h3>' . t('Replies') . '</h3>');
  $build['replies'] = array(
    '#theme' => 'table',
    '#header' => array(t('Reply'), t('Topic'), t('Posted'), t('Status')),
    '#rows' => $reply_rows,
    '#empty' => t('No replies.'),
  );
  $build['replies_pager'] = array('#theme' => 'pager', '#element' => 1);

  return $build;
}

/**
 * Form builder: the forum profile form on the account Forum tab.
 *
 * Lets the member set a public handle shown as their name on forum posts
 * instead of the account/login name, and upload a forum picture shown
 * beside their posts.
 *
 * @param object $account
 *   The account the settings belong to.
 *
 * @return array
 *   Form array.
 */
function entity_forum_posting_name_form($form, &$form_state, $account) {
  $form['#account'] = $account;
  $form['entity_forum_posting_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Forum posting name'),
    '#description' => t('Shown as your name on forum posts instead of your account name — use a handle if you prefer. Leave empty to use your account name.'),
    '#default_value' => isset($account->data['entity_forum_posting_name']) ? $account->data['entity_forum_posting_name'] : '',
    '#maxlength' => 60,
  );
  $current = entity_forum_format_avatar($account->uid);
  if ($current) {
    $form['entity_forum_avatar_preview'] = array(
      '#markup' => '<div class="entity-forum-avatar entity-forum-avatar-preview">' . $current . '</div>',
    );
  }
  $form['entity_forum_avatar'] = array(
    '#type' => 'managed_file',
    '#title' => t('Forum picture'),
    '#description' => t('Shown beside your name on forum posts. PNG, JPG or GIF, up to 2 MB.'),
    '#default_value' => entity_forum_user_avatar_fid($account),
    '#upload_location' => 'public://entity_forum/avatars',
    '#upload_validators' => array(
      'file_validate_extensions' => array('png jpg jpeg gif'),
      'file_validate_size' => array(2 * 1024 * 1024),
    ),
  );

  // Moderation control, shown only to moderators and user administrators:
  // hold this member's new posts for review, or suspend posting.
  if (entity_forum_user_is_moderator() || user_access('administer users')) {
    $form['entity_forum_posting_status'] = array(
      '#type' => 'select',
      '#title' => t('Posting status'),
      '#options' => entity_forum_posting_status_options(),
      '#default_value' => entity_forum_user_posting_status($account),
      '#description' => t('Moderation setting — visible to moderators only. Posts already held for review stay in the moderation queue when the status returns to normal.'),
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  return $form;
}

/**
 * Submit handler for entity_forum_posting_name_form().
 *
 * Saves the forum picture (entity_forum_avatar_save() handles the
 * managed-file bookkeeping), then the posting name — which also
 * refreshes the author_name snapshots on the member's existing posts via
 * entity_forum_posting_name_save().
 */
function entity_forum_posting_name_form_submit($form, &$form_state) {
  entity_forum_avatar_save($form['#account'], $form_state['values']['entity_forum_avatar']);
  entity_forum_posting_name_save($form['#account'], $form_state['values']['entity_forum_posting_name']);
  // Present only when the submitter is a moderator or user administrator.
  if (isset($form_state['values']['entity_forum_posting_status'])) {
    entity_forum_posting_status_save($form['#account'], $form_state['values']['entity_forum_posting_status']);
  }
  backdrop_set_message(t('Your forum settings have been saved.'));
}

/**
 * Renders a forum description through its text format.
 *
 * Accepts an entity or a plain DB row, since the forums index and sub-forum
 * lists work from light queries rather than loaded entities — both must
 * select description_format alongside description.
 *
 * A description saved before formats existed, or one from a row that somehow
 * lacks a format, falls back to the site's default format rather than being
 * printed raw: these values were entered as plain text and escaped on
 * output, so any markup in them was never live and must not become live now.
 *
 * @param object $forum
 *   A forum entity or a row with description and description_format.
 *
 * @return string
 *   Rendered description markup, or '' when empty.
 */
function _entity_forum_format_description($forum) {
  if (empty($forum->description)) {
    return '';
  }
  $format = !empty($forum->description_format) ? $forum->description_format : filter_default_format();
  return check_markup($forum->description, $format);
}
