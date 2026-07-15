<?php

/**
 * @file
 * Admin pages and entity forms for the Entity Forum module.
 */

/**
 * Page callback: admin overview of forums.
 *
 * @return string
 *   Rendered table of forums.
 */
function entity_forum_admin_forums() {
  backdrop_add_css(backdrop_get_path('module', 'entity_forum') . '/css/entity_forum_admin.css');

  $header = array(
    array('data' => t('Title'), 'field' => 'title', 'sort' => 'asc'),
    t('Parent'),
    t('Visibility'),
    t('Topics'),
    t('Status'),
    t('Operations'),
  );

  $query = db_select('entity_forum_forum', 'f')
    ->extend('TableSort')
    ->fields('f', array('forum_id', 'title', 'parent_id', 'visibility', 'status'))
    ->orderByHeader($header);
  $result = $query->execute();

  $forums_by_id = db_query('SELECT forum_id, title FROM {entity_forum_forum}')->fetchAllKeyed();

  $rows = array();
  foreach ($result as $forum) {
    $rows[] = array(
      l($forum->title, 'forum/' . $forum->forum_id),
      $forum->parent_id ? check_plain($forums_by_id[$forum->parent_id] ?? t('Missing (@id)', array('@id' => $forum->parent_id))) : t('- None -'),

      //'<span class="forum-visibility">' . check_plain($forum->visibility) . '</span>',
      ($forum->visibility === 'public') ?
        '<span class="forum-public">' . t('Public') . '</span>' :
        '<span class="forum-private">' . t('Private') . '</span>',

      '<div class="forum-id">' . entity_forum_forum_topic_count($forum->forum_id) . '</div>',

      $forum->status ? '<span class="forum-published">' . t('Published') . '</span>' :
        '<span class="forum-unpublished">' . t('Unpublished') . '</span>',

        _entity_forum_admin_operations('forum', $forum->forum_id),
    );
  }

  return theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'empty' => t('No forums yet. <a href="@add">Add the first forum</a>.', array(
      '@add' => url('admin/content/entity-forum/forums/add'),
    )),
  ));
}

/**
 * Page callback: admin overview of topics.
 *
 * @return string
 *   Rendered table of topics with pager.
 */
function entity_forum_admin_topics() {
  $header = array(
    array('data' => t('Title'), 'field' => 'title'),
    t('Forum'),
    array('data' => t('Author')),
    array('data' => t('Replies'), 'field' => 'reply_count'),
    array('data' => t('Last active'), 'field' => 'last_active_time', 'sort' => 'desc'),
    array('data' => t('Listing'), 'field' => 'listing'),
    array('data' => t('Status'), 'field' => 'status'),
    t('Operations'),
  );

  $query = db_select('entity_forum_topic', 't')
    ->extend('PagerDefault')
    ->extend('TableSort')
    ->fields('t', array('topic_id', 'title', 'forum_id', 'author_uid', 'author_name', 'reply_count', 'last_active_time', 'listing', 'status', 'pending'))
    ->orderByHeader($header)
    ->limit(50);
  $result = $query->execute();

  $forums_by_id = db_query('SELECT forum_id, title FROM {entity_forum_forum}')->fetchAllKeyed();

  // Short labels for the Listing column; 'normal' shows as a quiet dash.
  $listing_labels = array(
    ENTITY_FORUM_LISTING_NORMAL => t('Listed'),
    ENTITY_FORUM_LISTING_ADMIN_ONLY => t('Moderators only'),
    ENTITY_FORUM_LISTING_UNLISTED => t('Unlisted'),
  );

  $rows = array();
  foreach ($result as $topic) {
    $rows[] = array(
      l($topic->title, 'topic/' . $topic->topic_id),
      isset($forums_by_id[$topic->forum_id]) ? check_plain($forums_by_id[$topic->forum_id]) : t('N/A'),
      entity_forum_format_author($topic->author_uid, $topic->author_name),
      $topic->reply_count,
      format_date($topic->last_active_time, 'short'),
      isset($listing_labels[$topic->listing]) ? $listing_labels[$topic->listing] : check_plain($topic->listing),
      $topic->pending ? t('Awaiting review') : ($topic->status ? t('Published') : t('Unpublished')),
      _entity_forum_admin_operations('topic', $topic->topic_id),
    );
  }

  $output = theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'empty' => t('No topics available.'),
  ));
  $output .= theme('pager');
  return $output;
}

/**
 * Page callback: admin overview of replies.
 *
 * @return string
 *   The embedded forum_replies_admin view (filterable table + pager).
 */
function entity_forum_admin_replies() {
  // The overview is the forum_replies_admin view (shipped in config/,
  // built by Steve 2026-07-15), embedded so the columns and exposed
  // filters stay adjustable at admin/structure/views. The content
  // column's HTML is stripped in entity_forum_views_pre_render().
  backdrop_add_css(backdrop_get_path('module', 'entity_forum') . '/css/entity_forum_admin.css');
  return views_embed_view('forum_replies_admin', 'default');
}

/**
 * Builds the edit/delete operations cell for admin overview tables.
 *
 * @param string $type
 *   Path fragment: 'forum', 'topic' or 'reply'.
 * @param int $id
 *   Entity ID.
 *
 * @return string
 *   Rendered links.
 */
function _entity_forum_admin_operations($type, $id) {
  $base = "admin/content/entity-forum/$type/$id";
  return theme('links', array(
    'links' => array(
      'edit' => array('title' => t('Edit'), 'href' => "$base/edit"),
      'delete' => array('title' => t('Delete'), 'href' => "$base/delete"),
    ),
    'attributes' => array('class' => array('links', 'inline')),
  ));
}

/**
 * Returns forum titles keyed by forum_id, for select options.
 *
 * When excluding a forum (the one being edited), its whole descendant
 * sub-tree is excluded too — re-parenting a forum under one of its own
 * children would create a cycle.
 *
 * @param int|null $exclude_id
 *   A forum ID to leave out (e.g. the forum being edited, to prevent
 *   self-parenting), along with all of its descendants.
 *
 * @return array
 *   Options array for a select element.
 */
function entity_forum_forum_options($exclude_id = NULL) {
  $options = db_query('SELECT forum_id, title FROM {entity_forum_forum} ORDER BY title')->fetchAllKeyed();
  if ($exclude_id !== NULL) {
    unset($options[$exclude_id]);
    foreach (entity_forum_forum_descendants($exclude_id) as $descendant_id) {
      unset($options[$descendant_id]);
    }
  }
  return $options;
}

/**
 * Form builder: add/edit a forum.
 *
 * @param EntityForumForum|null $forum
 *   The forum to edit, or NULL to create a new one.
 */
function entity_forum_forum_form($form, &$form_state, $forum = NULL) {
  if (!isset($forum)) {
    $forum = entity_create('entity_forum_forum', array());
  }
  $form_state['forum'] = $forum;

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#maxlength' => 255,
    '#default_value' => $forum->title,
    '#required' => TRUE,
  );
  $form['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#description' => t('Plain text shown in forum listings.'),
    '#default_value' => (string) $forum->description,
    '#rows' => 3,
  );
  $form['parent_id'] = array(
    '#type' => 'select',
    '#title' => t('Parent forum'),
    '#options' => array(0 => t('- None -')) + entity_forum_forum_options($forum->id()),
    '#default_value' => $forum->parent_id,
  );
  $form['visibility'] = array(
    '#type' => 'select',
    '#title' => t('Visibility'),
    '#options' => array(
      'public' => t('Public'),
      'private' => t('Private (requires the "View private Entity Forums" permission)'),
    ),
    '#default_value' => $forum->visibility,
  );
  $form['weight'] = array(
    '#type' => 'number',
    '#title' => t('Weight'),
    '#description' => t('Forums are listed from lowest to highest weight.'),
    '#default_value' => $forum->weight,
  );
  $form['status'] = array(
    '#type' => 'checkbox',
    '#title' => t('Published'),
    '#default_value' => $forum->status,
  );
  $form['author_close'] = array(
    '#type' => 'checkbox',
    '#title' => t('Authors may close and reopen their own topics'),
    '#description' => t('Adds a Close/Reopen link to a topic for its author — for classified-ad forums, so a seller can mark an ad sold. Applies to this forum only, not its sub-forums.'),
    '#default_value' => !empty($forum->author_close),
  );

  // Widgets for any fields a site has attached to forums.
  field_attach_form('entity_forum_forum', $forum, $form, $form_state);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => $forum->isNew() ? t('Add forum') : t('Save forum'),
  );

  return $form;
}

/**
 * Validation for entity_forum_forum_form(): attached fields.
 */
function entity_forum_forum_form_validate($form, &$form_state) {
  field_attach_form_validate('entity_forum_forum', $form_state['forum'], $form, $form_state);
}

/**
 * Submit handler for entity_forum_forum_form().
 */
function entity_forum_forum_form_submit($form, &$form_state) {
  $forum = $form_state['forum'];
  $values = $form_state['values'];

  $is_new = $forum->isNew();
  $forum->title = $values['title'];
  $forum->description = $values['description'];
  $forum->parent_id = (int) $values['parent_id'];
  $forum->visibility = $values['visibility'];
  $forum->weight = (int) $values['weight'];
  $forum->status = (int) $values['status'];
  $forum->author_close = (int) !empty($values['author_close']);
  $forum->changed = REQUEST_TIME;
  field_attach_submit('entity_forum_forum', $forum, $form, $form_state);
  $forum->save();

  backdrop_set_message($is_new
    ? t('Forum %title has been created.', array('%title' => $forum->title))
    : t('Forum %title has been updated.', array('%title' => $forum->title)));
  $form_state['redirect'] = 'admin/content/entity-forum';
}

/**
 * Form builder: confirm deletion of a forum.
 *
 * @param EntityForumForum $forum
 *   The forum to delete.
 */
function entity_forum_forum_delete_form($form, &$form_state, $forum) {
  $form_state['forum'] = $forum;
  return confirm_form(
    $form,
    t('Delete forum %title?', array('%title' => $forum->title)),
    'admin/content/entity-forum',
    t('This action cannot be undone.'),
    t('Delete')
  );
}

/**
 * Validation for entity_forum_forum_delete_form().
 *
 * Refuses to delete a forum that still contains topics or child forums, to
 * avoid orphaning content.
 */
function entity_forum_forum_delete_form_validate($form, &$form_state) {
  $forum = $form_state['forum'];
  $topic_count = db_query('SELECT COUNT(topic_id) FROM {entity_forum_topic} WHERE forum_id = :id', array(':id' => $forum->id()))->fetchField();
  if ($topic_count > 0) {
    form_set_error('', t('Forum %title still contains @count topic(s). Delete or move them first.', array(
      '%title' => $forum->title,
      '@count' => $topic_count,
    )));
  }
  $child_count = db_query('SELECT COUNT(forum_id) FROM {entity_forum_forum} WHERE parent_id = :id', array(':id' => $forum->id()))->fetchField();
  if ($child_count > 0) {
    form_set_error('', t('Forum %title still has @count child forum(s). Delete or re-parent them first.', array(
      '%title' => $forum->title,
      '@count' => $child_count,
    )));
  }
}

/**
 * Submit handler for entity_forum_forum_delete_form().
 */
function entity_forum_forum_delete_form_submit($form, &$form_state) {
  $forum = $form_state['forum'];
  $title = $forum->title;
  $forum->delete();
  backdrop_set_message(t('Forum %title has been deleted.', array('%title' => $title)));
  $form_state['redirect'] = 'admin/content/entity-forum';
}

/**
 * Form builder: add/edit a topic (admin version with all controls).
 *
 * @param EntityForumTopic|null $topic
 *   The topic to edit, or NULL to create a new one.
 */
function entity_forum_topic_form($form, &$form_state, $topic = NULL) {
  if (!isset($topic)) {
    $topic = entity_create('entity_forum_topic', array());
  }
  $form_state['topic'] = $topic;

  $forum_options = entity_forum_forum_options();
  if (empty($forum_options)) {
    backdrop_set_message(t('Create a forum before adding topics.'), 'warning');
  }

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#maxlength' => 255,
    '#default_value' => $topic->title,
    '#required' => TRUE,
  );
  $form['content'] = array(
    '#type' => 'text_format',
    '#title' => t('Content'),
    '#default_value' => (string) $topic->content,
    '#format' => $topic->format,
    '#rows' => 10,
    // Node-attached discussion topics have an empty body — the node is
    // the opening post — so the body must stay optional for them.
    '#required' => empty($topic->node_id),
  );
  $form['forum_id'] = array(
    '#type' => 'select',
    '#title' => t('Forum'),
    '#options' => $forum_options,
    '#default_value' => $topic->forum_id,
    '#required' => TRUE,
  );
  $form['sticky'] = array(
    '#type' => 'checkbox',
    '#title' => t('Sticky (listed first in the forum)'),
    '#default_value' => $topic->sticky,
  );
  $form['closed'] = array(
    '#type' => 'checkbox',
    '#title' => t('Closed (no new replies)'),
    '#default_value' => $topic->closed,
  );
  $form['listing'] = array(
    '#type' => 'select',
    '#title' => t('Topic list visibility'),
    '#options' => entity_forum_topic_listing_options(),
    '#default_value' => $topic->listing,
    '#description' => t('"Only to moderators" holds the topic for moderation — the topic page itself also becomes moderator-only. "Not shown" keeps the topic off forum lists but leaves its page viewable (used for e.g. comment threads attached elsewhere).'),
  );
  $form['status'] = array(
    '#type' => 'checkbox',
    '#title' => t('Published'),
    '#default_value' => $topic->status,
  );

  $node_default = '';
  if (!empty($topic->node_id)) {
    $node_title = db_query('SELECT title FROM {node} WHERE nid = :nid', array(':nid' => $topic->node_id))->fetchField();
    $node_default = ($node_title !== FALSE ? $node_title : t('Unknown content')) . ' [nid:' . $topic->node_id . ']';
  }
  $form['node_link'] = array(
    '#type' => 'textfield',
    '#title' => t('Attached content'),
    '#autocomplete_path' => 'entity-forum/node-autocomplete',
    '#default_value' => $node_default,
    '#maxlength' => 300,
    '#description' => t('Attach this topic to a piece of content as its discussion thread, shown below the content (search by title). Usually combined with "Not shown in the forum topic list". Leave empty for a normal forum topic.'),
  );

  // Widgets for any fields a site has attached to topics.
  field_attach_form('entity_forum_topic', $topic, $form, $form_state);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => $topic->isNew() ? t('Add topic') : t('Save topic'),
  );

  return $form;
}

/**
 * Validation for entity_forum_topic_form(): attached content + fields.
 *
 * The "Attached content" value must come from the autocomplete
 * ("Title [nid:N]"), reference an existing node, and not collide with
 * another topic already attached to the same node (one discussion per
 * node). The parsed nid is stashed for the submit handler.
 */
function entity_forum_topic_form_validate($form, &$form_state) {
  $value = trim($form_state['values']['node_link']);
  $node_id = 0;
  if ($value !== '') {
    if (!preg_match('/\[nid:(\d+)\]\s*$/', $value, $matches)) {
      form_set_error('node_link', t('Pick the attached content from the autocomplete suggestions (the value must end in "[nid:N]"), or leave the field empty.'));
    }
    else {
      $node_id = (int) $matches[1];
      if (!db_query('SELECT 1 FROM {node} WHERE nid = :nid', array(':nid' => $node_id))->fetchField()) {
        form_set_error('node_link', t('The referenced content (nid @nid) does not exist.', array('@nid' => $node_id)));
      }
      else {
        $existing = db_query('SELECT topic_id FROM {entity_forum_topic} WHERE node_id = :nid AND topic_id <> :self', array(
          ':nid' => $node_id,
          ':self' => (int) ($form_state['topic']->id() ? $form_state['topic']->id() : 0),
        ))->fetchField();
        if ($existing) {
          form_set_error('node_link', t('Topic @id is already attached to that content — a node has one discussion thread.', array('@id' => $existing)));
        }
      }
    }
  }
  $form_state['entity_forum_node_id'] = $node_id;
  field_attach_form_validate('entity_forum_topic', $form_state['topic'], $form, $form_state);
}

/**
 * Submit handler for entity_forum_topic_form().
 */
function entity_forum_topic_form_submit($form, &$form_state) {
  $topic = $form_state['topic'];
  $values = $form_state['values'];

  $is_new = $topic->isNew();
  $topic->title = $values['title'];
  $topic->content = $values['content']['value'];
  $topic->format = $values['content']['format'];
  $topic->forum_id = (int) $values['forum_id'];
  $topic->sticky = (int) $values['sticky'];
  $topic->closed = (int) $values['closed'];
  $topic->listing = $values['listing'];
  $topic->status = (int) $values['status'];
  $topic->node_id = (int) $form_state['entity_forum_node_id'];
  $topic->changed = REQUEST_TIME;
  field_attach_submit('entity_forum_topic', $topic, $form, $form_state);
  $topic->save();

  backdrop_set_message($is_new
    ? t('Topic %title has been created.', array('%title' => $topic->title))
    : t('Topic %title has been updated.', array('%title' => $topic->title)));
  $form_state['redirect'] = 'topic/' . $topic->id();
}

/**
 * Form builder: confirm deletion of a topic (and its replies).
 *
 * @param EntityForumTopic $topic
 *   The topic to delete.
 */
function entity_forum_topic_delete_form($form, &$form_state, $topic) {
  $form_state['topic'] = $topic;
  $reply_count = db_query('SELECT COUNT(reply_id) FROM {entity_forum_reply} WHERE topic_id = :id', array(':id' => $topic->id()))->fetchField();
  return confirm_form(
    $form,
    t('Delete topic %title?', array('%title' => $topic->title)),
    'forum/' . $topic->forum_id,
    format_plural($reply_count,
      'One reply will also be deleted. This action cannot be undone.',
      '@count replies will also be deleted. This action cannot be undone.'),
    t('Delete')
  );
}

/**
 * Submit handler for entity_forum_topic_delete_form().
 */
function entity_forum_topic_delete_form_submit($form, &$form_state) {
  $topic = $form_state['topic'];
  $title = $topic->title;
  $forum_id = $topic->forum_id;
  // Replies are cascaded by entity_forum_entity_delete().
  $topic->delete();
  backdrop_set_message(t('Topic %title and its replies have been deleted.', array('%title' => $title)));
  $form_state['redirect'] = 'forum/' . $forum_id;
}

/**
 * Form builder: edit a reply (admin/author version).
 *
 * @param EntityForumReply $reply
 *   The reply to edit. Replies are created on the topic page, not here.
 */
function entity_forum_reply_form($form, &$form_state, $reply) {
  $form_state['reply'] = $reply;

  $form['content'] = array(
    '#type' => 'text_format',
    '#title' => t('Reply'),
    '#default_value' => (string) $reply->content,
    '#format' => $reply->format,
    '#rows' => 6,
    '#required' => TRUE,
  );
  $form['status'] = array(
    '#type' => 'checkbox',
    '#title' => t('Published'),
    '#default_value' => $reply->status,
    '#access' => user_access('administer entity forum reply'),
  );

  // Widgets for any fields a site has attached to replies.
  field_attach_form('entity_forum_reply', $reply, $form, $form_state);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save reply'),
  );

  return $form;
}

/**
 * Validation for entity_forum_reply_form(): attached fields.
 */
function entity_forum_reply_form_validate($form, &$form_state) {
  field_attach_form_validate('entity_forum_reply', $form_state['reply'], $form, $form_state);
}

/**
 * Submit handler for entity_forum_reply_form().
 */
function entity_forum_reply_form_submit($form, &$form_state) {
  $reply = $form_state['reply'];
  $values = $form_state['values'];

  $reply->content = $values['content']['value'];
  $reply->format = $values['content']['format'];
  if (isset($values['status'])) {
    $reply->status = (int) $values['status'];
  }
  $reply->changed = REQUEST_TIME;
  field_attach_submit('entity_forum_reply', $reply, $form, $form_state);
  $reply->save();

  backdrop_set_message(t('The reply has been updated.'));
  $form_state['redirect'] = array('topic/' . $reply->topic_id, array('fragment' => 'reply-' . $reply->id()));
}

/**
 * Form builder: confirm deletion of a reply.
 *
 * @param EntityForumReply $reply
 *   The reply to delete.
 */
function entity_forum_reply_delete_form($form, &$form_state, $reply) {
  $form_state['reply'] = $reply;
  return confirm_form(
    $form,
    t('Delete this reply?'),
    'topic/' . $reply->topic_id,
    t('This action cannot be undone.'),
    t('Delete')
  );
}

/**
 * Submit handler for entity_forum_reply_delete_form().
 */
function entity_forum_reply_delete_form_submit($form, &$form_state) {
  $reply = $form_state['reply'];
  $topic_id = $reply->topic_id;
  $reply->delete();
  backdrop_set_message(t('The reply has been deleted.'));
  $form_state['redirect'] = 'topic/' . $topic_id;
}

/**
 * Form builder: Entity Forum settings (admin/content/entity-forum/settings).
 *
 * Currently the default forum picture; intended to grow as more forum
 * settings are needed.
 *
 * @return array
 *   Form array.
 */
function entity_forum_settings_form($form, &$form_state) {
  $config = config('entity_forum.settings');

  $form['avatars'] = array(
    '#type' => 'fieldset',
    '#title' => t('Forum pictures'),
  );
  $form['avatars']['preview'] = array(
    '#markup' => '<div class="entity-forum-avatar-preview">' . entity_forum_format_default_avatar() . '</div>',
  );
  $form['avatars']['avatar_default_fid'] = array(
    '#type' => 'managed_file',
    '#title' => t('Default forum picture'),
    '#description' => t("Shown beside posts by members without their own picture, and by former members. Leave empty to use the module's built-in silhouette. PNG, JPG or GIF."),
    '#default_value' => (int) $config->get('avatar_default_fid'),
    '#upload_location' => 'public://entity_forum',
    '#upload_validators' => array(
      'file_validate_extensions' => array('png jpg jpeg gif'),
    ),
  );

  $form['htmx'] = array(
    '#type' => 'fieldset',
    '#title' => t('htmx library'),
    '#description' => t('The forum uses the <a href="@htmx">htmx</a> library for its modal reply form. The module ships its own copy; loading from a CDN instead lets the site use a newer htmx release without waiting for a module update.', array(
      '@htmx' => 'https://htmx.org/',
    )),
  );
  $form['htmx']['htmx_cdn'] = array(
    '#type' => 'checkbox',
    '#title' => t('Load htmx from a CDN'),
    '#default_value' => (int) $config->get('htmx_cdn'),
    '#description' => t("When unchecked, the module's bundled copy is used."),
  );
  $form['htmx']['htmx_cdn_url'] = array(
    '#type' => 'textfield',
    '#title' => t('CDN URL for htmx'),
    '#default_value' => (string) $config->get('htmx_cdn_url'),
    '#description' => t('For example %url.', array('%url' => 'https://unpkg.com/htmx.org@2.0.7/dist/htmx.min.js')),
    '#maxlength' => 255,
    '#states' => array(
      'visible' => array(':input[name="htmx_cdn"]' => array('checked' => TRUE)),
    ),
  );

  // Content discussions: the comments replacement. Each enabled content
  // type gets its discussion topics find-or-created (unlisted) in the
  // chosen forum and rendered below the node.
  $form['node_discussions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content discussions (comments replacement)'),
    '#description' => t('Show a forum-powered discussion below content instead of core comments. A hidden ("not listed") topic is created automatically in the chosen forum the first time the content is viewed; replies appear below the content with the same reply/report tools as the forum. Disable core comments on these content types yourself.'),
  );
  $forum_options = array(0 => t('- Disabled -')) + entity_forum_forum_options();
  $node_types_map = config_get('entity_forum.settings', 'node_types');
  if (!is_array($node_types_map)) {
    $node_types_map = array();
  }
  foreach (node_type_get_types() as $node_type) {
    $form['node_discussions']['node_type_' . $node_type->type] = array(
      '#type' => 'select',
      '#title' => t('@name discussions forum', array('@name' => $node_type->name)),
      '#options' => $forum_options,
      '#default_value' => isset($node_types_map[$node_type->type]) ? (int) $node_types_map[$node_type->type] : 0,
    );
  }

  // Topic scheduling: publish on a start date, close + delist after an
  // end date — built for classified-ad forums.
  $form['scheduling'] = array(
    '#type' => 'fieldset',
    '#title' => t('Topic scheduling (classified ads)'),
    '#description' => t('Publish topics when a start date arrives and close them (dropping them from topic lists) after an end date, using date fields attached to topics — add the fields at <a href="@fields">the topic field settings</a> and use each field\'s "Restrict to forums" option to scope them to your classifieds forums. Cron runs the transitions.', array(
      '@fields' => url('admin/structure/entity-forum'),
    )),
  );
  $date_field_options = array('' => t('- Disabled -'));
  foreach (field_info_instances('entity_forum_topic', 'entity_forum_topic') as $field_name => $instance) {
    $field_info = field_info_field($field_name);
    if (in_array($field_info['type'], array('date', 'datetime', 'datestamp'), TRUE)) {
      $date_field_options[$field_name] = t('@label (@name)', array('@label' => $instance['label'], '@name' => $field_name));
    }
  }
  $form['scheduling']['schedule_start_field'] = array(
    '#type' => 'select',
    '#title' => t('Start date field'),
    '#options' => $date_field_options,
    '#default_value' => (string) $config->get('schedule_start_field'),
    '#description' => t('A topic with a future start date stays unpublished (visible only to its author and moderators) until the date arrives.'),
  );
  $form['scheduling']['schedule_end_field'] = array(
    '#type' => 'select',
    '#title' => t('End date field'),
    '#options' => $date_field_options,
    '#default_value' => (string) $config->get('schedule_end_field'),
    '#description' => t('The topic stays visible through its end date, then is closed and dropped from topic lists (direct links keep working).'),
  );
  $form['scheduling']['schedule_duration'] = array(
    '#type' => 'number',
    '#title' => t('Default lifetime (days)'),
    '#min' => 0,
    '#default_value' => (int) $config->get('schedule_duration'),
    '#description' => t('Used when a topic has a start date but no end date: it closes this many days after the start. 0 means no automatic end.'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );
  return $form;
}

/**
 * Validation for entity_forum_settings_form(): the htmx CDN URL.
 *
 * The URL becomes a script tag on forum pages, so it must be an
 * absolute https URL when CDN loading is enabled.
 */
function entity_forum_settings_form_validate($form, &$form_state) {
  if (!empty($form_state['values']['htmx_cdn'])) {
    $url = trim($form_state['values']['htmx_cdn_url']);
    if ($url === '' || strpos($url, 'https://') !== 0 || !valid_url($url, TRUE)) {
      form_set_error('htmx_cdn_url', t('Enter an absolute https:// URL for the htmx CDN, or untick the CDN option to use the bundled copy.'));
    }
    $form_state['values']['htmx_cdn_url'] = $url;
  }
}

/**
 * Submit handler for entity_forum_settings_form().
 *
 * Persists the default picture with managed-file bookkeeping: a new
 * upload is made permanent and gets a usage record; the replaced upload
 * is released and deleted (the module's shipped SVG is untouched — it is
 * used whenever no file is configured).
 */
function entity_forum_settings_form_submit($form, &$form_state) {
  $config = config('entity_forum.settings');
  $old_fid = (int) $config->get('avatar_default_fid');
  $fid = (int) $form_state['values']['avatar_default_fid'];

  if ($fid !== $old_fid) {
    if ($fid && ($file = file_load($fid))) {
      $file->status = FILE_STATUS_PERMANENT;
      file_save($file);
      file_usage_add($file, 'entity_forum', 'default_avatar', 1);
    }
    if ($old_fid && ($old_file = file_load($old_fid))) {
      file_usage_delete($old_file, 'entity_forum', 'default_avatar', 1);
      file_delete($old_file->fid);
    }
  }

  $config->set('avatar_default_fid', $fid);
  $config->set('htmx_cdn', (int) $form_state['values']['htmx_cdn']);
  $config->set('htmx_cdn_url', trim($form_state['values']['htmx_cdn_url']));

  // Content discussions: store only the enabled types (type => forum).
  $node_types_map = array();
  foreach (node_type_get_types() as $node_type) {
    $key = 'node_type_' . $node_type->type;
    if (!empty($form_state['values'][$key])) {
      $node_types_map[$node_type->type] = (int) $form_state['values'][$key];
    }
  }
  $config->set('node_types', $node_types_map);

  // Topic scheduling (classified ads).
  $config->set('schedule_start_field', (string) $form_state['values']['schedule_start_field']);
  $config->set('schedule_end_field', (string) $form_state['values']['schedule_end_field']);
  $config->set('schedule_duration', max(0, (int) $form_state['values']['schedule_duration']));

  $config->save();
  backdrop_set_message(t('The configuration options have been saved.'));
}

/**
 * Page callback: autocomplete for the "Attached content" topic field.
 *
 * Matches node titles ("Title [nid:N]" results); the query carries the
 * node_access tag so the suggestions respect content access just like
 * core reference autocompletes.
 *
 * @param string $string
 *   The typed search string.
 *
 * @return array
 *   Matches, displayed as JSON by delivery callback
 *   backdrop_json_deliver().
 */
function entity_forum_node_autocomplete($string = '') {
  $matches = array();
  if ($string !== '') {
    $query = db_select('node', 'n')
      ->fields('n', array('nid', 'title'))
      ->condition('n.title', '%' . db_like($string) . '%', 'LIKE')
      ->orderBy('n.title')
      ->range(0, 10)
      ->addTag('node_access');
    foreach ($query->execute() as $node) {
      $matches[$node->title . ' [nid:' . $node->nid . ']'] = check_plain($node->title) . ' <small>[nid:' . $node->nid . ']</small>';
    }
  }
  return $matches;
}

/**
 * Page callback: the Forums structure landing page (admin/structure/entity-forum).
 *
 * Lists the three fieldable entity types with links to their Field UI
 * pages, so site builders can add fields (attachments, ratings, ...)
 * without any forum-specific code.
 *
 * @return array
 *   Render array.
 */
function entity_forum_structure_page() {
  $build = array();
  $rows = array();
  $types = array(
    'forum' => array(t('Forum'), t('Forum containers holding discussion topics.')),
    'topic' => array(t('Topic'), t('Discussion threads within a forum.')),
    'reply' => array(t('Reply'), t('Replies to forum topics.')),
  );
  $field_ui = module_exists('field_ui');
  foreach ($types as $path_part => $info) {
    $links = array();
    if ($field_ui) {
      $links[] = l(t('Manage fields'), 'admin/structure/entity-forum/' . $path_part . '/fields');
      $links[] = l(t('Manage display'), 'admin/structure/entity-forum/' . $path_part . '/display');
    }
    $rows[] = array(
      l($info[0], 'admin/structure/entity-forum/' . $path_part),
      $info[1],
      implode(' | ', $links),
    );
  }
  if (!$field_ui) {
    $build['notice'] = array(
      '#markup' => '<div class="messages warning">' . t('Enable the <em>Field UI</em> module to manage fields and display for the forum entity types.') . '</div>',
    );
  }
  $build['types'] = array(
    '#theme' => 'table',
    '#header' => array(t('Type'), t('Description'), t('Operations')),
    '#rows' => $rows,
  );
  return $build;
}

/**
 * Page callback: a single forum entity type's structure page.
 *
 * Field UI attaches its "Manage fields" and "Manage display" tabs to
 * this path (declared as the bundle admin path in hook_entity_info()).
 *
 * @param string $entity_type
 *   The entity type machine name.
 *
 * @return array
 *   Render array.
 */
function entity_forum_structure_bundle_page($entity_type) {
  $info = entity_get_info($entity_type);
  backdrop_set_title($info['label']);
  $fields = field_info_instances($entity_type, $entity_type);
  $items = array();
  foreach ($fields as $instance) {
    $items[] = check_plain($instance['label']) . ' (' . check_plain($instance['field_name']) . ')';
  }
  $build['description'] = array(
    '#markup' => '<p>' . check_plain($info['description']) . '</p>',
  );
  $build['fields'] = array(
    '#theme' => 'item_list',
    '#title' => t('Attached fields'),
    '#items' => $items,
    '#empty' => t('No fields attached yet. Use the Manage fields tab to add some.'),
  );
  return $build;
}
