<?php

/**
 * @file
 * Frontend page callbacks and forms for the Entity Forum module.
 */

/**
 * Page callback: the forum index — all visible forums.
 *
 * Private forums are hidden from users without the 'view private entity forum'
 * permission. Forums are listed hierarchically (children under parents).
 *
 * @return array
 *   Render array.
 */
function entity_forum_forums_page() {
  $forums = db_query('SELECT forum_id, title, description, parent_id, visibility, status FROM {entity_forum_forum} WHERE status = 1 ORDER BY weight, title')->fetchAll();

  // Filter out private forums the user may not see, then group by parent.
  $children = array();
  foreach ($forums as $forum) {
    if ($forum->visibility === 'private' && !user_access('view private entity forum') && !user_access('administer entity forum')) {
      continue;
    }
    $children[$forum->parent_id][] = $forum;
  }

  $rows = array();
  _entity_forum_index_rows($children, 0, 0, $rows);

  $build['forums'] = array(
    '#theme' => 'table',
    '#header' => array(t('Forum'), t('Topics'), t('Last activity')),
    '#rows' => $rows,
    '#empty' => t('No forums have been created yet.'),
    '#attributes' => array('class' => array('entity-forum-index')),
  );

  // Let other modules alter the forums index page.
  backdrop_alter('entity_forum_forums_page', $build);
  return $build;
}

/**
 * Builds forum index table rows depth-first so children follow parents.
 *
 * @param array $children
 *   Visible forums grouped by parent_id.
 * @param int $parent_id
 *   Parent being rendered.
 * @param int $depth
 *   Current nesting depth (used for indentation).
 * @param array $rows
 *   Table rows, built by reference.
 */
function _entity_forum_index_rows(array $children, $parent_id, $depth, array &$rows) {
  if (empty($children[$parent_id])) {
    return;
  }
  $counts = entity_forum_forum_counts();
  foreach ($children[$parent_id] as $forum) {
    $forum_counts = isset($counts[$forum->forum_id]) ? $counts[$forum->forum_id] : array('topic_count' => 0, 'last_active_time' => 0);

    $title = l($forum->title, 'forum/' . $forum->forum_id);
    if ($forum->visibility === 'private') {
      $title .= ' <span class="entity-forum-private">' . t('(private)') . '</span>';
    }
    $cell = '<div class="entity-forum-title" style="padding-left: ' . ($depth * 24) . 'px">' . $title;
    if (!empty($forum->description)) {
      $cell .= '<div class="entity-forum-description">' . check_plain($forum->description) . '</div>';
    }
    $cell .= '</div>';

    $rows[] = array(
      $cell,
      $forum_counts['topic_count'],
      $forum_counts['last_active_time'] ? format_date($forum_counts['last_active_time'], 'short') : t('Never'),
    );
    _entity_forum_index_rows($children, $forum->forum_id, $depth + 1, $rows);
  }
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
  backdrop_add_css(backdrop_get_path('module', 'entity_forum') . '/css/entity_forum.css');

  // Breadcrumb: Home > Forums > ancestor chain (the forum itself is the
  // page title, so it is not repeated as a trail link).
  _entity_forum_set_breadcrumb((int) $forum->parent_id);

  if (!empty($forum->description)) {
    $build['description'] = array(
      '#markup' => '<div class="entity-forum-description">' . check_plain($forum->description) . '</div>',
    );
  }

  // Sub-forums, if any: full rows with topic/post counts and last
  // activity (rolled up to include their own sub-forums), matching the
  // forums index rather than a plain bullet list.
  $subforums = db_query('SELECT forum_id, title, description, visibility FROM {entity_forum_forum} WHERE parent_id = :id AND status = 1 ORDER BY weight, title', array(':id' => $forum->id()))->fetchAll();
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
      $cell .= '<div class="entity-forum-description">' . check_plain($subforum->description) . '</div>';
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

  // Topic list: the entity_forum_topics view (sticky first, then latest
  // activity), embedded so site builders can adjust the columns at
  // admin/structure/views. Access checks, breadcrumbs, subforums and the
  // "Post new topic" button stay in this page callback.
  $build['topics'] = array(
    '#markup' => '<div class="entity-forum-topics">' . views_embed_view('entity_forum_topics', 'default', $forum->id()) . '</div>',
  );

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
  backdrop_add_css(backdrop_get_path('module', 'entity_forum') . '/css/entity_forum.css');

  if (!$topic->status) {
    backdrop_set_message(t('This topic is unpublished and only visible to administrators.'), 'warning');
    if (!user_access('administer entity forum topic')) {
      backdrop_access_denied();
      backdrop_exit();
    }
  }

  // Breadcrumb: Home > Forums > ancestor chain > the topic's forum.
  _entity_forum_set_breadcrumb((int) $topic->forum_id);

  // The opening post.
  $topic_fields = _entity_forum_render_fields('entity_forum_topic', array($topic->id() => $topic));
  $build['topic'] = array(
    '#prefix' => '<div class="entity-forum-topic-wrapper"><h2 class="entity-forum-topic-title">' . check_plain($topic->title) . '</h2>',
    '#suffix' => '</div>',
    '#markup' => _entity_forum_render_post(
      $topic->content,
      $topic->format,
      $topic->author_uid,
      $topic->created,
      'topic-' . $topic->id(),
      _entity_forum_post_links('topic', $topic),
      '',
      entity_forum_post_attachments('entity_forum_topic', $topic->id()),
      $topic->author_name,
      isset($topic_fields[$topic->id()]) ? $topic_fields[$topic->id()] : ''
    ),
  );

  // Replies, oldest first.
  $query = db_select('entity_forum_reply', 'r')
    ->extend('PagerDefault')
    ->fields('r')
    ->condition('r.topic_id', $topic->id())
    ->condition('r.status', 1)
    ->orderBy('r.created', 'ASC')
    ->orderBy('r.reply_id', 'ASC')
    ->limit(50);
  $replies = $query->execute()->fetchAllAssoc('reply_id');

  // Attached-field display needs the full entities for this page.
  $reply_entities = $replies ? entity_load('entity_forum_reply', array_keys($replies)) : array();
  $reply_fields = _entity_forum_render_fields('entity_forum_reply', $reply_entities);

  $items = '';
  foreach ($replies as $reply_id => $reply) {
    $prefix = '';
    if ($reply->reply_to_id) {
      $prefix = '<div class="entity-forum-reply-context">' . l(t('In reply to #@id', array('@id' => $reply->reply_to_id)), current_path(), array('fragment' => 'reply-' . $reply->reply_to_id)) . '</div>';
    }
    $items .= _entity_forum_render_post(
      $reply->content,
      $reply->format,
      $reply->author_uid,
      $reply->created,
      'reply-' . $reply->reply_id,
      _entity_forum_post_links('reply', $reply),
      $prefix,
      entity_forum_post_attachments('entity_forum_reply', $reply->reply_id),
      $reply->author_name,
      isset($reply_fields[$reply_id]) ? $reply_fields[$reply_id] : ''
    );
  }
  $build['replies'] = array('#markup' => $items);
  $build['pager'] = array('#theme' => 'pager');

  // Reply form.
  if ($topic->closed) {
    $build['closed'] = array(
      '#markup' => '<div class="entity-forum-closed">' . t('This topic is closed to new replies.') . '</div>',
    );
  }
  elseif (user_access('create entity forum reply')) {
    $build['reply_form'] = backdrop_get_form('entity_forum_reply_frontend_form', $topic);
  }
  elseif (user_is_anonymous()) {
    $build['login'] = array(
      '#markup' => '<div class="entity-forum-login">' . t('<a href="@login">Log in</a> to reply.', array(
        '@login' => url('user/login', array('query' => backdrop_get_destination())),
      )) . '</div>',
    );
  }

  // Let other modules alter the topic page (posts, reply form and all).
  backdrop_alter('entity_forum_topic_page', $build, $topic);
  return $build;
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
    $html = trim(backdrop_render($view));
    $rendered[$id] = $html === '' ? '' : '<div class="entity-forum-fields">' . $html . '</div>';
  }
  return $rendered;
}

/**
 * Renders a single forum post (topic body or reply).
 *
 * @param string $content
 *   Raw post body.
 * @param string|null $format
 *   Text format machine name; falls back to the site default.
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
 * @param string $attachments
 *   Rendered attachment list (see entity_forum_post_attachments()).
 * @param string $author_name
 *   Stored author_name snapshot, shown when the account is gone.
 * @param string $fields
 *   Rendered attached-field markup (see _entity_forum_render_fields()).
 *
 * @return string
 *   Sanitized post HTML.
 */
function _entity_forum_render_post($content, $format, $uid, $created, $html_id, $links = '', $prefix = '', $attachments = '', $author_name = '', $fields = '') {
  $output = '<div class="entity-forum-post" id="' . check_plain($html_id) . '">';
  $output .= $prefix;
  $avatar = entity_forum_format_avatar($uid);
  if ($avatar) {
    $output .= '<div class="entity-forum-avatar">' . $avatar . '</div>';
  }
  $output .= '<div class="entity-forum-byline">' . entity_forum_format_byline($uid, $created, $author_name) . '</div>';
  $output .= '<div class="entity-forum-content">' . check_markup($content, $format ? $format : filter_default_format(), '', TRUE) . '</div>';
  $output .= $fields;
  $output .= $attachments;
  $output .= entity_forum_format_signature($uid);
  if ($links) {
    $output .= '<div class="entity-forum-post-links">' . $links . '</div>';
  }
  $output .= '</div>';
  return $output;
}

/**
 * Renders the attachments belonging to a forum post.
 *
 * Attachments are managed files linked to the entity via {file_usage}
 * (module 'entity_forum') — the importer creates these; future upload UI
 * can use the same mechanism. Images render as thumbnails linking to the
 * full file; other types as download links with the file size.
 *
 * @param string $entity_type
 *   entity_forum_topic or entity_forum_reply.
 * @param int $entity_id
 *   The entity ID.
 *
 * @return string
 *   Rendered attachment list, or an empty string if there are none.
 */
function entity_forum_post_attachments($entity_type, $entity_id) {
  $fids = db_query("SELECT u.fid FROM {file_usage} u WHERE u.module = 'entity_forum' AND u.type = :type AND u.id = :id ORDER BY u.fid", array(
    ':type' => $entity_type,
    ':id' => $entity_id,
  ))->fetchCol();
  if (!$fids) {
    return '';
  }

  // Match the old bbPress behaviour: attachments are only listed for
  // logged-in members; visitors get a log-in prompt instead. (Note: the
  // files themselves are still public:// — direct URLs work until the
  // planned private-attachments task is done.)
  if (user_is_anonymous()) {
    return '<div class="entity-forum-attachments"><span class="entity-forum-attachments-label">' . t('Attachments:') . '</span> ' . t('You must be <a href="@login">logged in</a> to view attached files.', array(
      '@login' => url('user/login', array('query' => backdrop_get_destination())),
    )) . '</div>';
  }

  // @todo Give non-image attachments (e.g. .mov) a poster/placeholder
  //   image sized like the image thumbnails, and open all attachments in
  //   a Colorbox-style lightbox. Deferred pending a module choice.
  $items = array();
  foreach (file_load_multiple($fids) as $file) {
    $url = file_create_url($file->uri);
    if (strpos($file->filemime, 'image/') === 0) {
      // Images: just the clickable thumbnail — the filename/size metadata
      // is redundant next to the preview.
      $thumbnail = theme('image_style', array(
        'style_name' => 'thumbnail',
        'uri' => $file->uri,
        'alt' => $file->filename,
      ));
      $items[] = '<div class="entity-forum-attachment-inner">'
        . '<div class="entity-forum-attachment entity-forum-attachment--image">'
        . '<a href="' . check_plain($url) . '" class="entity-forum-attachment-image" title="' . check_plain($file->filename) . '">' . $thumbnail . '</a>'
        . '</div>'
        . '</div>';
    }
    else {
      // Non-image files: filename link only (their sole identifier); size
      // dropped.
      $items[] = '<div class="entity-forum-attachment-inner">'
        . '<div class="entity-forum-attachment entity-forum-attachment--file">'
        . '<div class="entity-forum-attachment-meta"><a href="' . check_plain($url) . '" class="entity-forum-attachment-file">' . check_plain($file->filename) . '</a></div>'
        . '</div>'
        . '</div>';
    }
  }

  return '<div class="entity-forum-attachments-label">' . t('Attachments:') . '</div><div class="entity-forum-attachments">'
    . '<div class="entity-forum-attachments-list">' . implode('', $items) . '</div>'
    . '</div>';
}

/**
 * Builds edit/delete links for a post, if the current user has access.
 *
 * @param string $type
 *   'topic' or 'reply'.
 * @param object $entity_or_row
 *   The entity (or a DB row with matching properties).
 *
 * @return string
 *   Rendered links, or an empty string.
 */
function _entity_forum_post_links($type, $entity_or_row) {
  $access_callback = "entity_forum_{$type}_access";
  $id = $type === 'topic' ? $entity_or_row->topic_id : $entity_or_row->reply_id;
  $links = array();
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
 * Form builder: post a new topic from within a forum.
 *
 * A lightweight version of the admin topic form: the forum is fixed and
 * moderation controls are hidden.
 *
 * @param EntityForumForum $forum
 *   The forum the topic will be posted in.
 */
function entity_forum_topic_frontend_form($form, &$form_state, $forum) {
  $form_state['forum'] = $forum;
  if (!isset($form_state['topic'])) {
    $form_state['topic'] = entity_create('entity_forum_topic', array(
      'forum_id' => (int) $forum->id(),
    ));
  }

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#maxlength' => 255,
    '#required' => TRUE,
  );
  $form['content'] = array(
    '#type' => 'text_format',
    '#title' => t('Content'),
    '#rows' => 10,
    '#required' => TRUE,
  );

  // Widgets for any fields a site has attached to topics.
  field_attach_form('entity_forum_topic', $form_state['topic'], $form, $form_state);

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
 */
function entity_forum_topic_frontend_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $topic = $form_state['topic'];
  $topic->title = $values['title'];
  $topic->content = $values['content']['value'];
  $topic->format = $values['content']['format'];
  field_attach_submit('entity_forum_topic', $topic, $form, $form_state);
  $topic->save();

  backdrop_set_message(t('Your topic %title has been posted.', array('%title' => $topic->title)));
  $form_state['redirect'] = 'topic/' . $topic->id();
}

/**
 * Form builder: post a reply from the topic page.
 *
 * Supports threading via a ?reply_to=N query parameter pointing at the
 * reply being answered.
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
  $form['content'] = array(
    '#type' => 'text_format',
    '#title' => t('Reply'),
    '#rows' => 6,
    '#required' => TRUE,
  );

  // Widgets for any fields a site has attached to replies.
  if (!isset($form_state['reply'])) {
    $form_state['reply'] = entity_create('entity_forum_reply', array(
      'topic_id' => (int) $topic->id(),
      'forum_id' => (int) $topic->forum_id,
    ));
  }
  field_attach_form('entity_forum_reply', $form_state['reply'], $form, $form_state);

  $form['actions'] = array('#type' => 'actions');
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
  $reply->content = $values['content']['value'];
  $reply->format = $values['content']['format'];
  $reply->reply_to_id = (int) $values['reply_to_id'];
  field_attach_submit('entity_forum_reply', $reply, $form, $form_state);
  $reply->save();

  backdrop_set_message(t('Your reply has been posted.'));
  $form_state['redirect'] = array('topic/' . $topic->id(), array('fragment' => 'reply-' . $reply->id()));
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
  backdrop_add_css(backdrop_get_path('module', 'entity_forum') . '/css/entity_forum.css');

  $build['posting_name_form'] = backdrop_get_form('entity_forum_posting_name_form', $account);
  $build['posting_name_form']['#weight'] = -10;

  $build['intro'] = array(
    '#markup' => '<p>' . t('Forum posts by %name.', array(
      '%name' => entity_forum_user_display_name($account),
    )) . '</p>',
  );

  // Topics.
  $topics = db_select('entity_forum_topic', 't')
    ->extend('PagerDefault')
    ->fields('t', array('topic_id', 'title', 'created', 'reply_count', 'status'))
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
      $topic->status ? t('Published') : t('Unpublished'),
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

  // Replies.
  $replies = db_select('entity_forum_reply', 'r')
    ->extend('PagerDefault')
    ->fields('r', array('reply_id', 'content', 'topic_id', 'created', 'status'))
    ->condition('r.author_uid', $account->uid)
    ->orderBy('r.created', 'DESC')
    ->limit(25);
  $replies->element(1);
  $reply_rows = array();
  foreach ($replies->execute() as $reply) {
    $topic_title = db_query('SELECT title FROM {entity_forum_topic} WHERE topic_id = :id', array(':id' => $reply->topic_id))->fetchField();
    $reply_rows[] = array(
      l(truncate_utf8(strip_tags($reply->content), 80, TRUE, TRUE), 'topic/' . $reply->topic_id, array('fragment' => 'reply-' . $reply->reply_id)),
      $topic_title !== FALSE ? check_plain($topic_title) : t('N/A'),
      format_date($reply->created, 'short'),
      $reply->status ? t('Published') : t('Unpublished'),
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
  backdrop_set_message(t('Your forum settings have been saved.'));
}
