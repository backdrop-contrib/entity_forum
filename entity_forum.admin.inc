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

  // Hierarchical, not sortable: forums are shown depth-first so children
  // sit under their parent, matching the front-end index. Siblings are
  // ordered by weight then title (the same order used on the index).
  $header = array(
    t('Title'),
    t('Parent'),
    t('Visibility'),
    t('Topics'),
    t('Status'),
    t('Listed'),
    t('Operations'),
  );

  // Every forum (published or not, listed or not) — admins see them all.
  $forums = db_query('SELECT forum_id, title, parent_id, visibility, status, listed FROM {entity_forum_forum} ORDER BY weight, title')->fetchAll();
  $forums_by_id = array();
  $children = array();
  foreach ($forums as $forum) {
    $forums_by_id[$forum->forum_id] = $forum->title;
    $children[$forum->parent_id][] = $forum;
  }

  $rows = array();
  $emitted = array();
  _entity_forum_admin_forum_rows($children, 0, 0, $rows, $emitted, $forums_by_id);

  // Orphans (parent forum deleted) are never reached from the root walk;
  // append each as a top-level row so admins always see every forum. The
  // Parent column flags them as "Missing (@id)".
  foreach ($forums as $forum) {
    if (!isset($emitted[$forum->forum_id])) {
      $emitted[$forum->forum_id] = TRUE;
      $rows[] = _entity_forum_admin_forum_row($forum, 0, $forums_by_id);
      _entity_forum_admin_forum_rows($children, $forum->forum_id, 1, $rows, $emitted, $forums_by_id);
    }
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
 * Emits admin forum table rows depth-first so children follow parents.
 *
 * @param array $children
 *   Forums grouped by parent_id (each an object with forum_id, title,
 *   parent_id, visibility, status, listed).
 * @param int $parent_id
 *   Parent whose children are being emitted.
 * @param int $depth
 *   Current nesting depth (indents the title cell).
 * @param array $rows
 *   Table rows, built by reference.
 * @param array $emitted
 *   forum_id => TRUE for forums already emitted, by reference — guards
 *   against a cyclic parent chain and drives orphan detection.
 * @param array $forums_by_id
 *   forum_id => title, for the Parent column.
 */
function _entity_forum_admin_forum_rows(array $children, $parent_id, $depth, array &$rows, array &$emitted, array $forums_by_id) {
  if (empty($children[$parent_id])) {
    return;
  }
  foreach ($children[$parent_id] as $forum) {
    if (isset($emitted[$forum->forum_id])) {
      // Already shown — a cyclic parent chain would otherwise loop.
      continue;
    }
    $emitted[$forum->forum_id] = TRUE;
    $rows[] = _entity_forum_admin_forum_row($forum, $depth, $forums_by_id);
    _entity_forum_admin_forum_rows($children, $forum->forum_id, $depth + 1, $rows, $emitted, $forums_by_id);
  }
}

/**
 * Builds a single admin forum overview row.
 *
 * @param object $forum
 *   Forum record (forum_id, title, parent_id, visibility, status, listed).
 * @param int $depth
 *   Nesting depth; indents the title so the hierarchy reads at a glance.
 * @param array $forums_by_id
 *   forum_id => title, for the Parent column.
 *
 * @return array
 *   The table row cells.
 */
function _entity_forum_admin_forum_row($forum, $depth, array $forums_by_id) {
  $title = l($forum->title, 'forum/' . $forum->forum_id);
  if ($depth > 0) {
    $title = '<span style="padding-left: ' . ($depth * 24) . 'px">' . $title . '</span>';
  }

  return array(
    $title,
    $forum->parent_id ? check_plain($forums_by_id[$forum->parent_id] ?? t('Missing (@id)', array('@id' => $forum->parent_id))) : t('- None -'),

    ($forum->visibility === 'public') ?
      '<span class="forum-public">' . t('Public') . '</span>' :
      '<span class="forum-private">' . t('Private') . '</span>',

    '<div class="forum-id">' . entity_forum_forum_topic_count($forum->forum_id) . '</div>',

    $forum->status ? '<span class="forum-published">' . t('Published') . '</span>' :
      '<span class="forum-unpublished">' . t('Unpublished') . '</span>',

    $forum->listed ? '<span class="forum-published">' . t('Listed') . '</span>' :
      '<span class="forum-unpublished">' . t('Unlisted') . '</span>',

    _entity_forum_admin_operations('forum', $forum->forum_id),
  );
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
    array('data' => t('Type'), 'field' => 'type'),
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
    ->fields('t', array('topic_id', 'title', 'type', 'forum_id', 'author_uid', 'author_name', 'reply_count', 'last_active_time', 'listing', 'status', 'pending'))
    ->orderByHeader($header)
    ->limit(50);
  $result = $query->execute();

  $forums_by_id = db_query('SELECT forum_id, title FROM {entity_forum_forum}')->fetchAllKeyed();
  $topic_types = entity_forum_topic_types();

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
      isset($topic_types[$topic->type]) ? check_plain($topic_types[$topic->type]->label) : check_plain($topic->type),
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
    '#type' => 'text_format',
    '#title' => t('Description'),
    '#description' => t('Shown in forum listings and at the top of the forum.'),
    '#default_value' => (string) $forum->description,
    '#format' => $forum->description_format,
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
    '#description' => t('Unpublished forums are hidden from the lists and accept no new topics, but the forum page stays viewable.'),
    '#default_value' => $forum->status,
  );
  $form['listed'] = array(
    '#type' => 'checkbox',
    '#title' => t('Appear in forum list'),
    '#description' => t('When unticked, the forum is hidden from the forums index and sub-forum lists but stays fully usable at its own URL — for a forum you feature under its own menu link. Independent of Published.'),
    '#default_value' => !empty($forum->listed),
  );
  $form['author_close'] = array(
    '#type' => 'checkbox',
    '#title' => t('Authors may close and reopen their own topics'),
    '#description' => t('Adds a Close/Reopen link to a topic for its author — so, for example, a seller can mark an ad sold. Applies to this forum only, not its sub-forums.'),
    '#default_value' => !empty($forum->author_close),
  );

  $current_type = entity_forum_forum_topic_type($forum);
  $topic_type_options = array(
    ENTITY_FORUM_FORUM_TOPIC_TYPE_NONE => t('- None: sub-forums only -'),
  ) + entity_forum_topic_type_labels();
  $form['topic_type'] = array(
    '#type' => 'select',
    '#title' => t('Topic type'),
    '#options' => $topic_type_options,
    '#default_value' => $current_type ? $current_type->type : ENTITY_FORUM_FORUM_TOPIC_TYPE_NONE,
    '#description' => t('The kind of topic posted in this forum — members are never asked, they just post. A type carries its own fields, so a classifieds forum can have a price while an ordinary forum does not. Add types at <a href="@types">the topic types page</a>. Choose <em>None</em> for a forum that only groups sub-forums, like a "Classified Adverts" heading above For Sale and Wanted: nobody can post in it. Applies to this forum only, not its sub-forums.', array(
      '@types' => url('admin/structure/entity-forum/topic-types'),
    )),
  );
  if (!$forum->isNew() && entity_forum_forum_topic_count($forum->id()) > 0) {
    $form['topic_type']['#description'] .= ' <strong>' . t('Changing this does NOT retype the topics already posted here — they keep the type they were created with.') . '</strong>';
  }

  $view_options = entity_forum_topic_list_view_options();
  $form['topic_view'] = array(
    '#type' => 'select',
    '#title' => t('Topic list view'),
    '#options' => array('' => t('- Default -')) + $view_options,
    '#default_value' => isset($view_options[$forum->topic_view]) ? $forum->topic_view : '',
    '#description' => t('The view display that renders this forum\'s list of topics. Copy the shipped topics view at <a href="@views">the views page</a> and amend it to give a forum its own list — an ad grid for a classifieds forum, say, while the others keep the standard topic table. Only displays built on topics and taking a forum ID argument are offered, since the forum is passed to the display as that argument.', array(
      '@views' => url('admin/structure/views'),
    )),
    // A container forum renders no topic list, so the choice is meaningless.
    '#states' => array(
      'invisible' => array(
        ':input[name="topic_type"]' => array('value' => ENTITY_FORUM_FORUM_TOPIC_TYPE_NONE),
      ),
    ),
  );
  // Say so rather than silently showing "Default": the list this forum has
  // been rendering is not the one it will render after the next save.
  if ($forum->topic_view !== '' && !isset($view_options[$forum->topic_view])) {
    $form['topic_view']['#description'] .= ' <strong>' . t('This forum was set to %view, which no longer exists — it is falling back to the default.', array(
      '%view' => $forum->topic_view,
    )) . '</strong>';
  }

  // Widgets for any fields a site has attached to forums.
  field_attach_form('entity_forum_forum', $forum, $form, $form_state);

  // Order title/description alongside the real fields, per Field UI.
  entity_forum_apply_extra_field_weights($form, 'entity_forum_forum', 'entity_forum_forum');

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => $forum->isNew() ? t('Add forum') : t('Save forum'),
  );

  return $form;
}

/**
 * Validation for entity_forum_forum_form(): container rule + attached fields.
 *
 * A forum cannot become a container while it still holds topics: they would
 * be stranded — still readable, but with nothing listing them and no way to
 * add to the discussion. Same principle as refusing to delete a topic type
 * that is still in use.
 */
function entity_forum_forum_form_validate($form, &$form_state) {
  $forum = $form_state['forum'];

  if (!$forum->isNew() && $form_state['values']['topic_type'] === ENTITY_FORUM_FORUM_TOPIC_TYPE_NONE) {
    // Every topic, not just the listed ones: entity_forum_forum_topic_count()
    // tallies only normal-listing topics, so a forum holding nothing but
    // held or unlisted topics would look empty and orphan them silently.
    $count = (int) db_query('SELECT COUNT(*) FROM {entity_forum_topic} WHERE forum_id = :id', array(
      ':id' => $forum->id(),
    ))->fetchField();
    if ($count) {
      form_set_error('topic_type', format_plural($count,
        'This forum cannot take <em>None</em> as its topic type: it still holds 1 topic. Move or delete it first.',
        'This forum cannot take <em>None</em> as its topic type: it still holds @count topics. Move or delete them first.'));
    }
  }

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
  $forum->description = $values['description']['value'];
  $forum->description_format = $values['description']['format'];
  $forum->parent_id = (int) $values['parent_id'];
  $forum->visibility = $values['visibility'];
  $forum->weight = (int) $values['weight'];
  $forum->status = (int) $values['status'];
  $forum->listed = (int) !empty($values['listed']);
  $forum->author_close = (int) !empty($values['author_close']);
  $forum->topic_type = $values['topic_type'];
  $forum->topic_view = $values['topic_view'];
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
function entity_forum_topic_form($form, &$form_state, $topic = NULL, $topic_type = NULL) {
  if (!isset($topic)) {
    // The type is the bundle, so it must be set before field_attach_form()
    // decides which widgets this form gets — hence it arrives in the URL
    // rather than as a select (the node/add/TYPE pattern).
    $topic = entity_create('entity_forum_topic', isset($topic_type)
      ? array('type' => $topic_type->type)
      : array());
  }
  $form_state['topic'] = $topic;

  // Which kind of topic this is; the type cannot change after creation
  // because the fields belong to it.
  $types = entity_forum_topic_types();
  $form['topic_type_display'] = array(
    '#type' => 'item',
    '#title' => t('Topic type'),
    '#markup' => isset($types[$topic->type]) ? check_plain($types[$topic->type]->label) : check_plain($topic->type),
  );

  // Only forums that take this type: the type is a property of the forum, so
  // offering the rest would only produce a validation error. On edit the
  // topic's existing forum is always included, even if the forum's type has
  // since been changed out from under it.
  $forum_options = entity_forum_forums_of_topic_type($topic->type);
  if (!$topic->isNew() && !empty($topic->forum_id) && !isset($forum_options[$topic->forum_id])) {
    $current = entity_load('entity_forum_forum', $topic->forum_id);
    if ($current) {
      $forum_options[$topic->forum_id] = $current->label();
    }
  }
  if (empty($forum_options)) {
    backdrop_set_message(t('No forum takes %type topics yet. Set a forum\'s topic type to %type first.', array(
      '%type' => isset($types[$topic->type]) ? $types[$topic->type]->label : $topic->type,
    )), 'warning');
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
  // Order title/content alongside the type's fields, per Field UI.
  entity_forum_apply_extra_field_weights($form, 'entity_forum_topic', $topic->type);

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
 *
 * Also checks the chosen forum accepts this topic's type: unlike the
 * frontend, where the forum is fixed and the route already gates the type,
 * an administrator picks the forum here after the type is decided.
 */
function entity_forum_topic_form_validate($form, &$form_state) {
  $topic = $form_state['topic'];
  if (!empty($form_state['values']['forum_id'])) {
    $forum = entity_load('entity_forum_forum', $form_state['values']['forum_id']);
    if ($forum) {
      $types = entity_forum_topic_types();
      $forum_type = entity_forum_forum_topic_type($forum);
      $topic_label = isset($types[$topic->type]) ? $types[$topic->type]->label : $topic->type;
      $forum_edit = url('admin/content/entity-forum/forum/' . $forum->id() . '/edit');

      if (!$forum_type) {
        // A container: takes no topics at all, of any type.
        form_set_error('forum_id', t('%forum only groups sub-forums and takes no topics. Choose one of its sub-forums, or give it a topic type on the <a href="@forum">forum</a>.', array(
          '%forum' => $forum->label(),
          '@forum' => $forum_edit,
        )));
      }
      elseif ($forum_type->type !== $topic->type) {
        form_set_error('forum_id', t('%forum takes %forum_type topics, but this is a %type topic. Choose a forum that takes %type topics, or change the type on the <a href="@forum">forum</a>.', array(
          '%forum' => $forum->label(),
          '%forum_type' => $forum_type->label,
          '%type' => $topic_label,
          '@forum' => $forum_edit,
        )));
      }
    }
  }

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
  entity_forum_apply_extra_field_weights($form, 'entity_forum_reply', $reply->type);

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

  // Content discussions: the comments replacement. Each placement maps a
  // content type to the forum its discussion topics are find-or-created
  // (unlisted) in and rendered below the node. Managed as a list of
  // placements (add/edit/delete) rather than a select per content type, so
  // the page stays readable on sites with many content types.
  $form['node_discussions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content discussions (comments replacement)'),
    '#description' => t('Show a forum-powered discussion below content instead of core comments. A hidden ("not listed") topic is created automatically in the chosen forum the first time the content is viewed; replies appear below the content with the same reply/report tools as the forum. Disable core comments on these content types yourself.'),
  );

  $node_types_map = config_get('entity_forum.settings', 'node_types');
  if (!is_array($node_types_map)) {
    $node_types_map = array();
  }
  $type_names = node_type_get_names();
  $forum_titles = entity_forum_forum_options();
  $scope_labels = array(
    'auto' => t('Every item of this type'),
    'item' => t('Only items the author enables'),
  );

  $rows = array();
  foreach (array_keys($node_types_map) as $type) {
    $placement = entity_forum_node_discussion_placement($type);
    $forum_id = $placement['forum'];
    // A placement whose content type or forum has since been deleted is shown
    // as "(missing)" so it can be found and removed, not silently hidden.
    $rows[] = array(
      isset($type_names[$type]) ? check_plain($type_names[$type]) : t('@type (missing)', array('@type' => $type)),
      isset($forum_titles[$forum_id]) ? check_plain($forum_titles[$forum_id]) : t('Forum @id (missing)', array('@id' => $forum_id)),
      $scope_labels[$placement['scope']],
      l(t('Edit'), 'admin/content/entity-forum/settings/discussions/edit/' . $type),
      l(t('Delete'), 'admin/content/entity-forum/settings/discussions/delete/' . $type),
    );
  }

  $form['node_discussions']['table'] = array(
    '#markup' => theme('table', array(
      'header' => array(t('Content type'), t('Discussion forum'), t('Show on'), t('Edit'), t('Delete')),
      'rows' => $rows,
      'empty' => t('No content discussions configured.'),
    )),
  );
  $form['node_discussions']['add'] = array(
    '#type' => 'link',
    '#title' => t('Add placement'),
    '#href' => 'admin/content/entity-forum/settings/discussions/add',
    '#attributes' => array('class' => array('button', 'button-action')),
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
 * Form builder: add or edit a content-discussion placement.
 *
 * A placement maps one content type to the forum its discussions live in.
 * On edit the content type is fixed (it is the config key); only the forum
 * changes. On add, only content types without an existing placement are
 * offered, so the list can never hold two rows for one type.
 *
 * @param string|null $type
 *   The content type machine name to edit, or NULL to add a new placement.
 *
 * @return array
 *   Form array.
 */
function entity_forum_node_discussion_form($form, &$form_state, $type = NULL) {
  $node_types_map = config_get('entity_forum.settings', 'node_types');
  if (!is_array($node_types_map)) {
    $node_types_map = array();
  }
  $type_names = node_type_get_names();
  $is_edit = ($type !== NULL);

  if ($is_edit) {
    if (!isset($node_types_map[$type])) {
      backdrop_set_message(t('That content discussion does not exist.'), 'error');
      backdrop_goto('admin/content/entity-forum/settings');
    }
    $form['type'] = array('#type' => 'value', '#value' => $type);
    $form['type_display'] = array(
      '#type' => 'item',
      '#title' => t('Content type'),
      '#markup' => isset($type_names[$type]) ? check_plain($type_names[$type]) : check_plain($type),
    );
  }
  else {
    // Offer only content types that are not already placed.
    $available = array_diff_key($type_names, $node_types_map);
    if (empty($available)) {
      $form['none'] = array(
        '#markup' => '<p>' . t('Every content type already has a discussion forum. Edit an existing placement instead.') . '</p>',
      );
      return $form;
    }
    $form['type'] = array(
      '#type' => 'select',
      '#title' => t('Content type'),
      '#description' => t('The content whose discussion appears below it. Remember to disable core comments on this type yourself.'),
      '#options' => $available,
      '#required' => TRUE,
    );
  }

  $placement = $is_edit ? entity_forum_node_discussion_placement($type) : NULL;

  $form['forum_id'] = array(
    '#type' => 'select',
    '#title' => t('Discussion forum'),
    '#description' => t('The forum the discussion topics are created in.'),
    '#options' => entity_forum_forum_options(),
    '#default_value' => $placement ? $placement['forum'] : NULL,
    '#required' => TRUE,
  );

  $form['scope'] = array(
    '#type' => 'radios',
    '#title' => t('Show on'),
    '#options' => array(
      'auto' => t('Every item of this type'),
      'item' => t('Only items the author enables'),
    ),
    '#default_value' => $placement ? $placement['scope'] : 'auto',
    '#required' => TRUE,
    '#description' => t('<strong>Every item</strong> creates a discussion automatically the first time each piece of content is viewed. <strong>Only items the author enables</strong> adds a checkbox to the content edit form, so a discussion appears only where it is switched on — use this to give just one page a discussion. Disable core comments on this content type yourself either way.'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => 'admin/content/entity-forum/settings',
    '#attributes' => array('class' => array('button')),
  );
  return $form;
}

/**
 * Submit handler for entity_forum_node_discussion_form().
 *
 * Writes the single (content type => forum id) pair into the node_types map,
 * leaving other placements untouched.
 */
function entity_forum_node_discussion_form_submit($form, &$form_state) {
  $config = config('entity_forum.settings');
  $node_types_map = $config->get('node_types');
  if (!is_array($node_types_map)) {
    $node_types_map = array();
  }

  $type = $form_state['values']['type'];
  $node_types_map[$type] = array(
    'forum' => (int) $form_state['values']['forum_id'],
    'scope' => $form_state['values']['scope'],
  );

  $config->set('node_types', $node_types_map);
  $config->save();

  backdrop_set_message(t('The content discussion has been saved.'));
  $form_state['redirect'] = 'admin/content/entity-forum/settings';
}

/**
 * Confirmation form: delete a content-discussion placement.
 *
 * Removes the mapping only — existing discussion topics and their replies are
 * left in place; the content simply stops rendering its discussion.
 *
 * @param string $type
 *   The content type machine name whose placement is being deleted.
 *
 * @return array
 *   A confirm_form() array.
 */
function entity_forum_node_discussion_delete_form($form, &$form_state, $type) {
  $node_types_map = config_get('entity_forum.settings', 'node_types');
  if (!is_array($node_types_map) || !isset($node_types_map[$type])) {
    backdrop_set_message(t('That content discussion does not exist.'), 'error');
    backdrop_goto('admin/content/entity-forum/settings');
  }
  $type_names = node_type_get_names();
  $label = isset($type_names[$type]) ? $type_names[$type] : $type;

  $form['type'] = array('#type' => 'value', '#value' => $type);

  return confirm_form(
    $form,
    t('Remove the discussion forum for %type?', array('%type' => $label)),
    'admin/content/entity-forum/settings',
    t('Content of this type will stop showing a forum discussion. Existing discussion topics and their replies are kept.'),
    t('Remove'),
    t('Cancel')
  );
}

/**
 * Submit handler for entity_forum_node_discussion_delete_form().
 */
function entity_forum_node_discussion_delete_form_submit($form, &$form_state) {
  $config = config('entity_forum.settings');
  $node_types_map = $config->get('node_types');
  if (!is_array($node_types_map)) {
    $node_types_map = array();
  }

  unset($node_types_map[$form_state['values']['type']]);

  $config->set('node_types', $node_types_map);
  $config->save();

  backdrop_set_message(t('The content discussion has been removed.'));
  $form_state['redirect'] = 'admin/content/entity-forum/settings';
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
  $field_ui = module_exists('field_ui');

  // Forums are single-bundle: fields hang straight off the entity type.
  $types = array(
    'forum' => array(t('Forum'), t('Forum containers holding discussion topics.')),
  );
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

  // Topics and replies are bundleable: their fields belong to a topic type,
  // not to topics/replies as a whole, so both rows lead to the type list
  // rather than straight to Field UI.
  $topic_types = entity_forum_topic_types();
  $manage = l(format_plural(count($topic_types), 'Manage 1 topic type', 'Manage @count topic types'), 'admin/structure/entity-forum/topic-types');
  $rows[] = array(
    l(t('Topic'), 'admin/structure/entity-forum/topic-types'),
    t('Discussion threads within a forum. Topics have types — add a type to give a kind of topic its own fields.'),
    $manage,
  );
  $rows[] = array(
    l(t('Reply'), 'admin/structure/entity-forum/topic-types'),
    t('Replies to forum topics. A reply takes its type from the topic it is posted in, so its fields live on that topic type — see each type\'s <em>Replies</em> tab. For example, an offer amount on replies to a classified ad.'),
    $manage,
  );
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

/**
 * Page callback: the topic type list (admin/structure/entity-forum/topic-types).
 *
 * @return array
 *   Render array.
 */
function entity_forum_topic_types_page() {
  $build = array();
  $field_ui = module_exists('field_ui');
  $rows = array();

  foreach (entity_forum_topic_types() as $machine_name => $topic_type) {
    $path = 'admin/structure/entity-forum/topic-types/' . str_replace('_', '-', $machine_name);
    $operations = array(l(t('Edit'), $path));
    if ($field_ui) {
      $operations[] = l(t('Manage fields'), $path . '/fields');
      $operations[] = l(t('Manage display'), $path . '/display');
    }
    if ($machine_name !== ENTITY_FORUM_TOPIC_TYPE_DEFAULT) {
      $operations[] = l(t('Delete'), $path . '/delete');
    }
    $usage = entity_forum_topic_type_usage($machine_name);
    $rows[] = array(
      l($topic_type->label, $path),
      array('data' => check_plain($machine_name), 'class' => array('entity-forum-machine-name')),
      $topic_type->description === '' ? '—' : check_plain($topic_type->description),
      format_plural($usage, '1 topic', '@count topics'),
      implode(' | ', $operations),
    );
  }

  if (!$field_ui) {
    $build['notice'] = array(
      '#markup' => '<div class="messages warning">' . t('Enable the <em>Field UI</em> module to add fields to topic types.') . '</div>',
    );
  }
  $build['types'] = array(
    '#theme' => 'table',
    '#header' => array(t('Name'), t('Machine name'), t('Description'), t('Topics'), t('Operations')),
    '#rows' => $rows,
  );
  return $build;
}

/**
 * Form builder: add or edit a topic type.
 *
 * The machine name is locked once the type exists: it is the bundle name
 * every field instance on the type is keyed by, and the value stored in
 * {entity_forum_topic}.type on every topic of that type. Renaming it would
 * orphan both.
 *
 * @param object|null $topic_type
 *   The topic type to edit, or NULL to add a new one.
 */
function entity_forum_topic_type_form($form, &$form_state, $topic_type = NULL) {
  $is_new = !isset($topic_type);
  $form_state['topic_type'] = $topic_type;
  $form['#tree'] = FALSE;

  $form['label'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#description' => t('The human-readable name of this topic type, e.g. "Classified".'),
    '#default_value' => $is_new ? '' : $topic_type->label,
    '#maxlength' => 255,
    '#required' => TRUE,
  );
  $form['type'] = array(
    '#type' => 'machine_name',
    '#default_value' => $is_new ? '' : $topic_type->type,
    '#maxlength' => 32,
    '#disabled' => !$is_new,
    '#machine_name' => array(
      'exists' => 'entity_forum_topic_type_load',
      'source' => array('label'),
    ),
    '#description' => t('A unique machine-readable name. Cannot be changed once the type exists.'),
  );
  $form['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#description' => t('Shown to moderators on the topic type list.'),
    '#default_value' => $is_new ? '' : $topic_type->description,
    '#rows' => 3,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => $is_new ? t('Save topic type') : t('Save'),
  );
  return $form;
}

/**
 * Submit handler for entity_forum_topic_type_form().
 */
function entity_forum_topic_type_form_submit($form, &$form_state) {
  $type = (object) array(
    'type' => $form_state['values']['type'],
    'label' => $form_state['values']['label'],
    'description' => $form_state['values']['description'],
  );
  entity_forum_topic_type_save($type);
  backdrop_set_message(t('The topic type %label has been saved.', array('%label' => $type->label)));
  $form_state['redirect'] = 'admin/structure/entity-forum/topic-types';
}

/**
 * Form builder: confirm deletion of a topic type.
 *
 * @param object $topic_type
 *   The topic type to delete.
 */
function entity_forum_topic_type_delete_form($form, &$form_state, $topic_type) {
  $form_state['topic_type'] = $topic_type;
  $usage = entity_forum_topic_type_usage($topic_type->type);

  // The default type is the fallback every topic without a type lands on
  // (see ENTITY_FORUM_TOPIC_TYPE_DEFAULT); deleting it would only make
  // entity_forum_topic_types() synthesise it again.
  if ($topic_type->type === ENTITY_FORUM_TOPIC_TYPE_DEFAULT) {
    $form['message'] = array(
      '#markup' => '<p>' . t('The %label type cannot be deleted: it is the default topic type.', array('%label' => $topic_type->label)) . '</p>',
    );
    return $form;
  }

  // Deleting a type with topics would leave them pointing at a bundle that
  // no longer exists, and every field API call on them would throw.
  if ($usage > 0) {
    $form['message'] = array(
      '#markup' => '<p>' . format_plural($usage,
        'The %label type cannot be deleted: 1 topic still uses it. Move or delete that topic first.',
        'The %label type cannot be deleted: @count topics still use it. Move or delete those topics first.',
        array('%label' => $topic_type->label)) . '</p>',
    );
    return $form;
  }

  return confirm_form(
    $form,
    t('Are you sure you want to delete the topic type %label?', array('%label' => $topic_type->label)),
    'admin/structure/entity-forum/topic-types',
    t('Any fields attached to this type will also be removed. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for entity_forum_topic_type_delete_form().
 */
function entity_forum_topic_type_delete_form_submit($form, &$form_state) {
  $topic_type = $form_state['topic_type'];

  // Re-check under the same rules as the form build: the confirm page may
  // have been sitting open while a topic was created on this type.
  if ($topic_type->type === ENTITY_FORUM_TOPIC_TYPE_DEFAULT || entity_forum_topic_type_usage($topic_type->type) > 0) {
    backdrop_set_message(t('The topic type %label could not be deleted.', array('%label' => $topic_type->label)), 'error');
  }
  else {
    field_attach_delete_bundle('entity_forum_topic', $topic_type->type);
    entity_forum_topic_type_delete($topic_type->type);
    backdrop_set_message(t('The topic type %label has been deleted.', array('%label' => $topic_type->label)));
  }
  $form_state['redirect'] = 'admin/structure/entity-forum/topic-types';
}

/**
 * Page callback: add a topic as an administrator.
 *
 * Mirrors the frontend flow: with one topic type this is the form, with
 * several it lists them first. The type travels in the URL because it is the
 * bundle and so decides the form's fields.
 *
 * @return array|string
 *   The topic form, or a render array listing the types.
 */
function entity_forum_admin_topic_add_page() {
  $types = entity_forum_topic_types();

  if (count($types) === 1) {
    return backdrop_get_form('entity_forum_topic_form', NULL, reset($types));
  }

  $items = array();
  foreach ($types as $machine_name => $topic_type) {
    $path = 'admin/content/entity-forum/topics/add/' . str_replace('_', '-', $machine_name);
    $item = '<strong>' . l($topic_type->label, $path) . '</strong>';
    if ($topic_type->description !== '') {
      $item .= '<div class="description">' . check_plain($topic_type->description) . '</div>';
    }
    $items[] = $item;
  }

  return array(
    'intro' => array(
      '#markup' => '<p>' . t('Choose the kind of topic to add.') . '</p>',
    ),
    'types' => array(
      '#theme' => 'item_list',
      '#items' => $items,
    ),
  );
}

/**
 * Page callback: the "Replies" tab on a topic type.
 *
 * Exists so Field UI has a page to hang its Manage fields / Manage display
 * tabs on for the reply bundle (see the reply bundles in
 * entity_forum_entity_info()). Replies are not configurable in their own
 * right — their type is their topic's type — so this page only explains
 * itself and points at the field tabs.
 *
 * @param object $topic_type
 *   The topic type, loaded by entity_forum_topic_type_load().
 *
 * @return array
 *   Render array.
 */
function entity_forum_topic_type_replies_page($topic_type) {
  $path = 'admin/structure/entity-forum/topic-types/' . str_replace('_', '-', $topic_type->type) . '/replies';
  $build = array();
  $build['intro'] = array(
    '#markup' => '<p>' . t('Fields here appear on replies posted in %type topics — an offer amount on replies to a classified ad, say, which would make no sense on an ordinary reply. A reply takes its type from the topic it belongs to, so there is nothing to choose: post a reply in a %type topic and it is a %type reply.', array(
      '%type' => $topic_type->label,
    )) . '</p>',
  );
  if (module_exists('field_ui')) {
    $build['links'] = array(
      '#theme' => 'item_list',
      '#items' => array(
        l(t('Manage fields'), $path . '/fields'),
        l(t('Manage display'), $path . '/display'),
      ),
    );
  }
  else {
    $build['notice'] = array(
      '#markup' => '<div class="messages warning">' . t('Enable the <em>Field UI</em> module to add fields to replies.') . '</div>',
    );
  }
  return $build;
}
