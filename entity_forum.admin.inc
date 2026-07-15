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
      check_plain($forum->visibility),
      entity_forum_forum_topic_count($forum->forum_id),
      $forum->status ? t('Published') : t('Unpublished'),
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
    array('data' => t('Status'), 'field' => 'status'),
    t('Operations'),
  );

  $query = db_select('entity_forum_topic', 't')
    ->extend('PagerDefault')
    ->extend('TableSort')
    ->fields('t', array('topic_id', 'title', 'forum_id', 'author_uid', 'author_name', 'reply_count', 'last_active_time', 'status'))
    ->orderByHeader($header)
    ->limit(50);
  $result = $query->execute();

  $forums_by_id = db_query('SELECT forum_id, title FROM {entity_forum_forum}')->fetchAllKeyed();

  $rows = array();
  foreach ($result as $topic) {
    $rows[] = array(
      l($topic->title, 'topic/' . $topic->topic_id),
      isset($forums_by_id[$topic->forum_id]) ? check_plain($forums_by_id[$topic->forum_id]) : t('N/A'),
      entity_forum_format_author($topic->author_uid, $topic->author_name),
      $topic->reply_count,
      format_date($topic->last_active_time, 'short'),
      $topic->status ? t('Published') : t('Unpublished'),
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
 *   Rendered table of replies with pager.
 */
function entity_forum_admin_replies() {
  $header = array(
    t('Content'),
    t('Topic'),
    t('Author'),
    array('data' => t('Created'), 'field' => 'created', 'sort' => 'desc'),
    array('data' => t('Status'), 'field' => 'status'),
    t('Operations'),
  );

  $query = db_select('entity_forum_reply', 'r')
    ->extend('PagerDefault')
    ->extend('TableSort')
    ->fields('r', array('reply_id', 'content', 'topic_id', 'author_uid', 'author_name', 'created', 'status'))
    ->orderByHeader($header)
    ->limit(50);
  $result = $query->execute();

  $rows = array();
  foreach ($result as $reply) {
    $topic_title = db_query('SELECT title FROM {entity_forum_topic} WHERE topic_id = :id', array(':id' => $reply->topic_id))->fetchField();
    $snippet = truncate_utf8(strip_tags($reply->content), 100, TRUE, TRUE);
    $rows[] = array(
      l($snippet, 'topic/' . $reply->topic_id, array('fragment' => 'reply-' . $reply->reply_id)),
      $topic_title !== FALSE ? check_plain($topic_title) : t('N/A'),
      entity_forum_format_author($reply->author_uid, $reply->author_name),
      format_date($reply->created, 'short'),
      $reply->status ? t('Published') : t('Unpublished'),
      _entity_forum_admin_operations('reply', $reply->reply_id),
    );
  }

  $output = theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'empty' => t('No replies available.'),
  ));
  $output .= theme('pager');
  return $output;
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
    '#required' => TRUE,
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
  $form['status'] = array(
    '#type' => 'checkbox',
    '#title' => t('Published'),
    '#default_value' => $topic->status,
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
 * Validation for entity_forum_topic_form(): attached fields.
 */
function entity_forum_topic_form_validate($form, &$form_state) {
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
  $topic->status = (int) $values['status'];
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

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );
  return $form;
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
  $config->save();
  backdrop_set_message(t('The configuration options have been saved.'));
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
