<?php

/**
 * @file
 * Admin pages and entity forms for the Entity Forum module.
 */

/**
 * Returns forum titles keyed by forum_id, in tree order.
 *
 * Ordered depth-first (each forum followed by its own sub-forums, siblings
 * by weight then title) so a picker reads in the same order as the forums
 * index rather than as one flat alphabetical run.
 *
 * When excluding a forum (the one being edited), its whole descendant
 * sub-tree is excluded too, re-parenting a forum under one of its own
 * children would create a cycle.
 *
 * @param int|null $exclude_id
 *   A forum ID to leave out (e.g. the forum being edited, to prevent
 *   self-parenting), along with all of its descendants.
 * @param bool $indent
 *   Prefix each title with one dash per level of nesting, the Backdrop
 *   idiom for showing hierarchy in a select. Off by default because the
 *   settings page uses this as a TITLE LOOKUP for a table cell, where the
 *   dashes would render as literal text; select elements pass TRUE.
 *
 * @return array
 *   Options array for a select element.
 */
function entity_forum_forum_options($exclude_id = NULL, $indent = FALSE) {
  $forums = db_query('SELECT forum_id, title, parent_id FROM {entity_forum_forum} ORDER BY weight, title')->fetchAll();

  $excluded = array();
  if ($exclude_id !== NULL) {
    $excluded[(int) $exclude_id] = TRUE;
    foreach (entity_forum_forum_descendants($exclude_id) as $descendant_id) {
      $excluded[(int) $descendant_id] = TRUE;
    }
  }

  // Group by parent. A forum whose parent is missing or has been excluded is
  // treated as a root so it can never drop out of the list entirely - the
  // same tolerance of messy data as the forums-index hierarchy style.
  $known = array();
  foreach ($forums as $forum) {
    $known[(int) $forum->forum_id] = TRUE;
  }
  $children = array();
  foreach ($forums as $forum) {
    if (isset($excluded[(int) $forum->forum_id])) {
      continue;
    }
    $parent = (int) $forum->parent_id;
    if (!isset($known[$parent]) || isset($excluded[$parent])) {
      $parent = 0;
    }
    $children[$parent][] = $forum;
  }

  $options = array();
  _entity_forum_forum_options_walk($children, 0, 0, $indent, $options);
  return $options;
}

/**
 * Appends one level of the forum tree to a select options array.
 *
 * Recursive helper for entity_forum_forum_options(). Carries a visited set
 * so a cycle in parent_id (corrupt data, never produced by the UI) exhausts
 * rather than recursing forever.
 *
 * @param array $children
 *   Forum rows grouped by parent ID.
 * @param int $parent
 *   The parent whose children to append.
 * @param int $depth
 *   Current nesting depth, 0 at the top level.
 * @param bool $indent
 *   Whether to prefix titles with dashes.
 * @param array $options
 *   The options array being built, by reference.
 * @param array $seen
 *   Forum IDs already emitted, by reference; the cycle guard.
 */
function _entity_forum_forum_options_walk($children, $parent, $depth, $indent, &$options, &$seen = array()) {
  if (empty($children[$parent])) {
    return;
  }
  foreach ($children[$parent] as $forum) {
    $forum_id = (int) $forum->forum_id;
    if (isset($seen[$forum_id])) {
      continue;
    }
    $seen[$forum_id] = TRUE;
    $options[$forum_id] = $indent && $depth
      ? str_repeat('-', $depth) . ' ' . $forum->title
      : $forum->title;
    _entity_forum_forum_options_walk($children, $forum_id, $depth + 1, $indent, $options, $seen);
  }
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
    '#options' => array(0 => t('- None -')) + entity_forum_forum_options($forum->id(), TRUE),
    '#default_value' => $forum->parent_id,
  );

  // Icon controls sit right after the parent selector, because the sensible
  // icon size depends on whether this is a top-level forum or a sub-forum.
  $form['icon_fid'] = array(
    '#type' => 'managed_file',
    '#title' => t('Icon'),
    '#description' => t('An optional icon shown beside the forum name in the forums index, sub-forum lists and at the top of the forum page. Leave empty for no icon. PNG, JPG, GIF or SVG.'),
    '#default_value' => (int) $forum->icon_fid,
    '#upload_location' => 'public://entity_forum',
    '#upload_validators' => array(
      'file_validate_extensions' => array('png jpg jpeg gif svg'),
    ),
  );

  // Per-forum icon image style, used wherever this forum's icon appears (index,
  // sub-forum lists and its own page). Any image style on the site is offered;
  // empty follows the shipped small style. Nothing is forced: a top-level forum
  // is free to use a large style to stand out, a sub-forum is best kept small
  // but may pick anything and be tidied up with CSS. The icon renders at
  // whatever the chosen style produces - no separate "size" setting.
  $icon_style_options = array('' => t('- Default (small) -'));
  foreach (image_styles() as $name => $style_info) {
    $icon_style_options[$name] = isset($style_info['label']) ? $style_info['label'] : $name;
  }
  $form['icon_style'] = array(
    '#type' => 'select',
    '#title' => t('Icon size'),
    '#options' => $icon_style_options,
    '#default_value' => isset($icon_style_options[$forum->icon_style]) ? $forum->icon_style : '',
    '#description' => t('The image style the icon is shown at, everywhere this forum appears. A <strong>top-level forum</strong> can use a large style like <em>Forum icon (large)</em> to stand out. For a <strong>sub-forum</strong>, the small <em>Default</em> usually sits better in lists - but you are free to choose any style and fine-tune the result with CSS. Create your own sizes at <a href="@styles">the image styles page</a>.', array(
      '@styles' => url('admin/config/media/image-styles'),
    )),
  );
  // Say so rather than silently reverting to Default: the size this forum has
  // been rendering at is not the one it will use after saving.
  if ($forum->icon_style !== '' && !isset($icon_style_options[$forum->icon_style])) {
    $form['icon_style']['#description'] .= ' <strong>' . t('This forum was set to the %style style, which no longer exists - it is falling back to the default.', array(
      '%style' => $forum->icon_style,
    )) . '</strong>';
  }
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
    '#description' => t('When unticked, the forum is hidden from the forums index and sub-forum lists, <strong>and so is everything beneath it</strong>, but the whole branch stays fully usable at its own URLs - for a section you feature under its own menu link or page. Sub-forums are still listed on their own parent\'s page, so the branch browses normally once you are inside it. Independent of Published.'),
    '#default_value' => !empty($forum->listed),
  );
  $form['author_close'] = array(
    '#type' => 'checkbox',
    '#title' => t('Authors may close and reopen their own topics'),
    '#description' => t('Adds a Close/Reopen link to a topic for its author - so, for example, a member can mark their own support question resolved. Applies to this forum only, not its sub-forums.'),
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
    '#description' => t('The kind of topic posted in this forum - members are never asked, they just post. A type carries its own fields, so a support forum can require a product-version field while an ordinary forum does not. Add types at <a href="@types">the topic types page</a>. Choose <em>None</em> for a forum that only groups sub-forums, like a "Support" heading above Installation and Usage: nobody can post in it. Applies to this forum only, not its sub-forums.', array(
      '@types' => url('admin/structure/entity-forum/topic-types'),
    )),
  );
  if (!$forum->isNew() && entity_forum_forum_topic_count($forum->id()) > 0) {
    $form['topic_type']['#description'] .= ' <strong>' . t('Changing this does NOT retype the topics already posted here - they keep the type they were created with.') . '</strong>';
  }

  $view_options = entity_forum_topic_list_view_options();
  $form['topic_view'] = array(
    '#type' => 'select',
    '#title' => t('Topic list view'),
    '#options' => array('' => t('- Default -')) + $view_options,
    '#default_value' => isset($view_options[$forum->topic_view]) ? $forum->topic_view : '',
    '#description' => t('The view display that renders this forum\'s list of topics. Copy the shipped topics view at <a href="@views">the views page</a> and amend it to give a forum its own list - a thumbnail grid for a photo-gallery forum, say, while the others keep the standard topic table. Only displays built on topics and taking a forum ID argument are offered, since the forum is passed to the display as that argument.', array(
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
    $form['topic_view']['#description'] .= ' <strong>' . t('This forum was set to %view, which no longer exists - it is falling back to the default.', array(
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
 * be stranded, still readable, but with nothing listing them and no way to
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
  // Read the stored icon before overwriting it, so the managed-file
  // bookkeeping below can release the file that is being replaced or cleared.
  $old_icon_fid = (int) $forum->icon_fid;
  $new_icon_fid = (int) $values['icon_fid'];
  $forum->title = $values['title'];
  $forum->description = $values['description']['value'];
  $forum->description_format = $values['description']['format'];
  $forum->icon_fid = $new_icon_fid;
  $forum->icon_style = $values['icon_style'];
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

  // After save, so a brand-new forum already has the ID the file usage is
  // keyed on. Makes the new file permanent + adds its usage, and releases the
  // replaced/cleared one.
  entity_forum_forum_icon_apply($forum->id(), $old_icon_fid, $new_icon_fid);

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

  // Everything the delete would take with it: the forum, its sub-forums, and
  // every topic and reply inside the whole sub-tree. Counted once here and
  // reused by validate/submit via $form_state.
  $counts = _entity_forum_forum_cascade_counts($forum->id());
  $form_state['cascade'] = $counts;

  // An empty leaf forum: a plain one-shot delete.
  if (_entity_forum_forum_cascade_is_empty($counts)) {
    return confirm_form(
      $form,
      t('Delete forum %title?', array('%title' => $forum->title)),
      'admin/content/entity-forum',
      t('This action cannot be undone.'),
      t('Delete')
    );
  }

  // A non-empty forum (has sub-forums and/or content): offer to delete the
  // whole sub-tree, behind an explicit confirmation. Deleting through the
  // entity layer (not a raw table drop) is what releases attached files.
  $items = array();
  if ($counts['child_count']) {
    $items[] = format_plural($counts['child_count'], '1 sub-forum', '@count sub-forums');
  }
  if ($counts['topic_count']) {
    $items[] = format_plural($counts['topic_count'], '1 topic', '@count topics');
  }
  if ($counts['reply_count']) {
    $items[] = format_plural($counts['reply_count'], '1 reply', '@count replies');
  }
  $description = t('This permanently deletes %title and everything inside it - !list - including any file attachments on those posts. This cannot be undone.', array(
    '%title' => $forum->title,
    '!list' => implode(', ', $items),
  ));

  $form = confirm_form(
    $form,
    t('Delete forum %title and all its contents?', array('%title' => $forum->title)),
    'admin/content/entity-forum',
    $description,
    t('Delete everything')
  );

  // A deliberate second action so a bulk content deletion cannot happen on a
  // single mis-click.
  $form['confirm_cascade'] = array(
    '#type' => 'checkbox',
    '#title' => t('Yes, permanently delete this forum and everything inside it.'),
    '#default_value' => 0,
    '#weight' => 1,
  );

  return $form;
}

/**
 * Validation for entity_forum_forum_delete_form().
 *
 * The empty-forum case needs no validation (a plain delete). The cascade case
 * requires the confirmation checkbox, so a non-empty forum is never wiped on a
 * single click.
 */
function entity_forum_forum_delete_form_validate($form, &$form_state) {
  if (_entity_forum_forum_cascade_is_empty($form_state['cascade'])) {
    return;
  }
  if (empty($form_state['values']['confirm_cascade'])) {
    form_set_error('confirm_cascade', t('Tick the box to confirm you want to delete this forum and all of its contents.'));
  }
}

/**
 * Submit handler for entity_forum_forum_delete_form().
 *
 * Empty forum: delete it directly. Non-empty forum: run a batch that deletes
 * the sub-tree's topics in chunks (each cascading to its replies, reports and
 * file usages) and then the now-empty forums, batched because a large forum
 * holds thousands of posts.
 */
function entity_forum_forum_delete_form_submit($form, &$form_state) {
  $forum = $form_state['forum'];
  $title = $forum->title;

  if (_entity_forum_forum_cascade_is_empty($form_state['cascade'])) {
    $forum->delete();
    backdrop_set_message(t('Forum %title has been deleted.', array('%title' => $title)));
    $form_state['redirect'] = 'admin/content/entity-forum';
    return;
  }

  $batch = array(
    'title' => t('Deleting forum %title and its contents', array('%title' => $title)),
    'operations' => array(
      array('entity_forum_forum_cascade_delete_batch_op', array($form_state['cascade']['forum_ids'], $title)),
    ),
    'finished' => 'entity_forum_forum_cascade_delete_finished',
    'file' => backdrop_get_path('module', 'entity_forum') . '/entity_forum.admin.inc',
  );
  batch_set($batch);
  $form_state['redirect'] = 'admin/content/entity-forum';
}

/**
 * Returns the sub-tree IDs and content counts for a forum delete.
 *
 * @param int $forum_id
 *   The forum being deleted.
 *
 * @return array
 *   - forum_ids: the forum plus every descendant forum ID.
 *   - child_count: number of descendant forums.
 *   - topic_count: topics in the whole sub-tree.
 *   - reply_count: replies to those topics.
 */
function _entity_forum_forum_cascade_counts($forum_id) {
  $forum_ids = entity_forum_forum_subtree_ids($forum_id);
  $topic_count = (int) db_query('SELECT COUNT(topic_id) FROM {entity_forum_topic} WHERE forum_id IN (:ids)', array(
    ':ids' => $forum_ids,
  ))->fetchField();
  $reply_count = (int) db_query('SELECT COUNT(r.reply_id) FROM {entity_forum_reply} r INNER JOIN {entity_forum_topic} t ON r.topic_id = t.topic_id WHERE t.forum_id IN (:ids)', array(
    ':ids' => $forum_ids,
  ))->fetchField();
  return array(
    'forum_ids' => $forum_ids,
    'child_count' => count($forum_ids) - 1,
    'topic_count' => $topic_count,
    'reply_count' => $reply_count,
  );
}

/**
 * Whether a cascade-count set describes an empty leaf forum.
 *
 * @param array $counts
 *   A set from _entity_forum_forum_cascade_counts().
 *
 * @return bool
 *   TRUE when the forum has no sub-forums, topics or replies.
 */
function _entity_forum_forum_cascade_is_empty(array $counts) {
  return $counts['child_count'] === 0 && $counts['topic_count'] === 0 && $counts['reply_count'] === 0;
}

/**
 * Batch operation: delete a forum sub-tree's content, then its forums.
 *
 * Re-entrant: each pass deletes a chunk of topics (each topic delete cascades
 * to its replies, reports and file usages through the entity layer), and the
 * final pass, once no topics remain, deletes the now-empty forums.
 *
 * @param array $forum_ids
 *   The forum plus every descendant forum ID.
 * @param string $title
 *   The root forum's title, for the completion message.
 * @param array $context
 *   Batch context.
 */
function entity_forum_forum_cascade_delete_batch_op($forum_ids, $title, &$context) {
  $chunk = 50;
  if (!isset($context['sandbox']['topics_total'])) {
    $context['sandbox']['topics_total'] = (int) db_query('SELECT COUNT(topic_id) FROM {entity_forum_topic} WHERE forum_id IN (:ids)', array(
      ':ids' => $forum_ids,
    ))->fetchField();
    $context['sandbox']['topics_done'] = 0;
    $context['results']['title'] = $title;
    $context['results']['forums'] = count($forum_ids);
    $context['results']['topics'] = $context['sandbox']['topics_total'];
  }

  $deleted = entity_forum_forum_delete_topics_chunk($forum_ids, $chunk);
  if ($deleted) {
    $context['sandbox']['topics_done'] += $deleted;
    $total = max(1, $context['sandbox']['topics_total']);
    // Cap below 1 so the forum-deletion pass below still gets to run.
    $context['finished'] = min(0.99, $context['sandbox']['topics_done'] / $total);
    $context['message'] = t('Deleted @done of @total topics…', array(
      '@done' => $context['sandbox']['topics_done'],
      '@total' => $context['sandbox']['topics_total'],
    ));
    return;
  }

  // No topics left in the sub-tree, remove the forums and finish.
  entity_forum_forum_delete_forums($forum_ids);
  $context['finished'] = 1;
}

/**
 * Batch 'finished' callback for the forum cascade delete.
 */
function entity_forum_forum_cascade_delete_finished($success, $results, $operations) {
  if ($success) {
    backdrop_set_message(t('Deleted forum %title along with its @f forum(s) and @t topic(s), and all their replies and attachments.', array(
      '%title' => $results['title'],
      '@f' => $results['forums'],
      '@t' => $results['topics'],
    )));
  }
  else {
    backdrop_set_message(t('The forum deletion did not finish. Some content may remain - you can run the delete again to clear it.'), 'error');
  }
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
    // decides which widgets this form gets, hence it arrives in the URL
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
    '#description' => t('"Only to moderators" holds the topic for moderation - the topic page itself also becomes moderator-only. "Not shown" keeps the topic off forum lists but leaves its page viewable (used for e.g. comment threads attached elsewhere).'),
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

  // The body (entity_forum_body) and any other fields attached to this type.
  field_attach_form('entity_forum_topic', $topic, $form, $form_state);
  // Order the title alongside the type's fields, per Field UI.
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
          form_set_error('node_link', t('Topic @id is already attached to that content - a node has one discussion thread.', array('@id' => $existing)));
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
    entity_forum_forum_path($topic->forum_id),
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
  $form_state['redirect'] = entity_forum_forum_path($forum_id);
}

/**
 * Form builder: edit a reply (admin/author version).
 *
 * @param EntityForumReply $reply
 *   The reply to edit. Replies are created on the topic page, not here.
 */
function entity_forum_reply_form($form, &$form_state, $reply) {
  $form_state['reply'] = $reply;

  $form['status'] = array(
    '#type' => 'checkbox',
    '#title' => t('Published'),
    '#default_value' => $reply->status,
    '#access' => user_access('administer entity forum reply'),
  );

  // The body (entity_forum_body, labelled "Reply") and any other fields
  // attached to this reply type.
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

  // Starter forums: the demo forums seeded at install (General Discussion, a
  // Classified Ads container and its Items for Sale child). Offered for removal
  // only while at least one still exists, a site that wants a clean slate can
  // clear them out without touching the classified type/fields/view, which
  // stay as reusable building blocks.
  $starter_ids = entity_forum_starter_forum_ids();
  if ($starter_ids) {
    $titles = array();
    foreach ($starter_ids as $id) {
      $forum = entity_load('entity_forum_forum', $id);
      if ($forum) {
        $titles[] = check_plain($forum->title);
      }
    }
    $form['starter'] = array(
      '#type' => 'fieldset',
      '#title' => t('Starter forums'),
      '#description' => t('The example forums installed with the module: !list. They demonstrate a plain forum and a fielded classified section. Remove them once you have your own forums set up - the classified topic type, its fields and the ad grid view are left in place for you to reuse.', array(
        '!list' => theme('item_list', array('items' => $titles)),
      )),
    );
    $form['starter']['remove'] = array(
      '#type' => 'link',
      '#title' => t('Remove pre-installed forums'),
      '#href' => 'admin/content/entity-forum/settings/remove-starter-forums',
      '#attributes' => array('class' => array('button')),
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );
  return $form;
}

/**
 * Returns the IDs of the starter forums that still exist.
 *
 * The install records the seeded forum IDs in entity_forum.settings; this
 * filters them to those still present, so the settings-page removal action is
 * offered only while there is something to remove (and never re-offers forums
 * a site has already deleted by hand).
 *
 * @return int[]
 *   Forum IDs, in the order they were seeded (parent before child), filtered
 *   to existing forums.
 */
function entity_forum_starter_forum_ids() {
  $ids = config_get('entity_forum.settings', 'starter_forum_ids');
  if (!is_array($ids) || !$ids) {
    return array();
  }
  $existing = db_query('SELECT forum_id FROM {entity_forum_forum} WHERE forum_id IN (:ids)', array(
    ':ids' => $ids,
  ))->fetchCol();
  $existing = array_map('intval', $existing);
  // Preserve the recorded order (parent before child) so removal can delete
  // children first.
  return array_values(array_intersect(array_map('intval', $ids), $existing));
}

/**
 * Form builder: confirm removal of the starter forums.
 *
 * Deletes only the forums seeded at install and still empty. A forum a site
 * has actually used (it holds topics, or has grown its own sub-forums) is kept
 * and reported, rather than taking members' content down with the demo, the
 * same protection as the ordinary forum delete form, applied per forum.
 */
function entity_forum_remove_starter_forums_form($form, &$form_state) {
  return confirm_form(
    $form,
    t('Remove the pre-installed starter forums?'),
    'admin/content/entity-forum/settings',
    t('The example forums (General Discussion, Classified Ads and Items for Sale) will be deleted. Any of them that you have posted topics into, or added your own sub-forums under, is kept. The classified topic type, its fields and the ad grid view are not removed.'),
    t('Remove forums')
  );
}

/**
 * Submit handler for entity_forum_remove_starter_forums_form().
 *
 * Deletes the recorded starter forums that are empty, children first so a
 * container is emptied before it is removed. Forums holding topics or carrying
 * non-starter children are skipped and named. The recorded ID list is cleared
 * of everything successfully removed.
 */
function entity_forum_remove_starter_forums_form_submit($form, &$form_state) {
  // Deepest first: reversing the seeded order (parent-before-child) deletes a
  // child before its container, so the container is childless when its turn
  // comes.
  $ids = array_reverse(entity_forum_starter_forum_ids());
  $removed = array();
  $kept = array();

  foreach ($ids as $id) {
    $forum = entity_load('entity_forum_forum', $id);
    if (!$forum) {
      continue;
    }
    $has_topics = (int) db_query('SELECT COUNT(topic_id) FROM {entity_forum_topic} WHERE forum_id = :id', array(':id' => $id))->fetchField();
    $has_children = (int) db_query('SELECT COUNT(forum_id) FROM {entity_forum_forum} WHERE parent_id = :id', array(':id' => $id))->fetchField();
    if ($has_topics || $has_children) {
      $kept[] = check_plain($forum->title);
      continue;
    }
    $forum->delete();
    $removed[$id] = check_plain($forum->title);
  }

  // Drop the removed forums from the recorded list; keep any that survived so
  // the action stays available until they too are gone (or deleted by hand).
  $remaining = array_values(array_diff(
    (array) config_get('entity_forum.settings', 'starter_forum_ids'),
    array_keys($removed)
  ));
  config_set('entity_forum.settings', 'starter_forum_ids', $remaining);

  if ($removed) {
    backdrop_set_message(t('Removed the starter forum(s): !list.', array(
      '!list' => implode(', ', $removed),
    )));
  }
  if ($kept) {
    backdrop_set_message(t('Kept !list - still in use (topics posted, or sub-forums added). Empty them first if you want them gone.', array(
      '!list' => implode(', ', $kept),
    )), 'warning');
  }
  $form_state['redirect'] = 'admin/content/entity-forum/settings';
}

/**
 * Submit handler for entity_forum_settings_form().
 *
 * Persists the default picture with managed-file bookkeeping: a new
 * upload is made permanent and gets a usage record; the replaced upload
 * is released and deleted (the module's shipped SVG is untouched, it is
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
    '#options' => entity_forum_forum_options(NULL, TRUE),
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
    '#description' => t('<strong>Every item</strong> creates a discussion automatically the first time each piece of content is viewed. <strong>Only items the author enables</strong> adds a checkbox to the content edit form, so a discussion appears only where it is switched on - use this to give just one page a discussion. Disable core comments on this content type yourself either way.'),
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
 * Removes the mapping only, existing discussion topics and their replies are
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
  // not to topics/replies as a whole, so the Topic row leads to the type list
  // rather than straight to Field UI (the Reply row is handled below).
  $topic_types = entity_forum_topic_types();
  $manage = l(format_plural(count($topic_types), 'Manage 1 topic type', 'Manage @count topic types'), 'admin/structure/entity-forum/topic-types');
  $rows[] = array(
    l(t('Topic'), 'admin/structure/entity-forum/topic-types'),
    t('Discussion threads within a forum. Topics have types - add a type to give a kind of topic its own fields.'),
    $manage,
  );
  // A reply's type is its topic's type, so reply fields live on each topic
  // type's Replies tab, not on a reply-types page of their own. The Reply row
  // leads to the reply fields overview, its own landing, one row per type
  // with reply-only operations, never to the topic-types list: sending both
  // rows to the same list read as "replies have no fields of their own".
  $rows[] = array(
    l(t('Reply'), 'admin/structure/entity-forum/replies'),
    t('Replies to forum topics. A reply takes its type from the topic it is posted in, so its fields live on that topic type - a collection separate from the topic\'s own fields. For example, an attendee count on replies to an event.'),
    l(t('Manage reply fields'), 'admin/structure/entity-forum/replies'),
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
    // Reply fields for this type live on its Replies tab; expose it here so
    // reply fields are reachable in one click even with several topic types.
    $operations[] = l(t('Replies'), $path . '/replies');
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
 * Page callback: the reply fields overview (admin/structure/entity-forum/replies).
 *
 * The reply-side twin of entity_forum_topic_types_page(), in the same table
 * shape. A reply's type is derived from its topic, so the reply bundles ARE
 * the topic types, one row per type, but the operations here are the
 * reply-only Field UI pages, never the topic's. Deliberately NO Edit
 * operation: a reply bundle has no settings of its own (its "edit" is the
 * topic type's edit form), and linking that here would send two rows to the
 * same form, the exact confusion this page exists to remove.
 *
 * @return array
 *   Render array.
 */
function entity_forum_reply_types_page() {
  $build = array();
  $field_ui = module_exists('field_ui');
  $rows = array();

  foreach (entity_forum_topic_types() as $machine_name => $topic_type) {
    $path = 'admin/structure/entity-forum/topic-types/' . str_replace('_', '-', $machine_name) . '/replies';
    $operations = array();
    if ($field_ui) {
      $operations[] = l(t('Manage fields'), $path . '/fields');
      $operations[] = l(t('Manage display'), $path . '/display');
    }
    $usage = entity_forum_reply_type_usage($machine_name);
    $rows[] = array(
      l($topic_type->label, $path),
      array('data' => check_plain($machine_name), 'class' => array('entity-forum-machine-name')),
      format_plural($usage, '1 reply', '@count replies'),
      implode(' | ', $operations),
    );
  }

  $build['intro'] = array(
    '#markup' => '<p>' . t('A reply takes its type from the topic it is posted in, so reply fields belong to a topic type - a collection separate from the topic\'s own fields. Pick the type whose replies you want to shape.') . '</p>',
  );
  if (!$field_ui) {
    $build['notice'] = array(
      '#markup' => '<div class="messages warning">' . t('Enable the <em>Field UI</em> module to add fields to replies.') . '</div>',
    );
  }
  $build['types'] = array(
    '#theme' => 'table',
    '#header' => array(t('Name'), t('Machine name'), t('Replies'), t('Operations')),
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
    '#description' => t('The human-readable name of this topic type, e.g. "Support" or "Event".'),
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
  // Adding, not editing: $form_state['topic_type'] is NULL for a new type.
  $is_new = empty($form_state['topic_type']);
  $type = (object) array(
    'type' => $form_state['values']['type'],
    'label' => $form_state['values']['label'],
    'description' => $form_state['values']['description'],
  );
  entity_forum_topic_type_save($type);
  // A new type gets a body field on its topic and reply bundles, like core
  // adds a body to a new content type. Only on create: re-adding on every
  // edit would resurrect a body a forum builder had deliberately deleted.
  if ($is_new) {
    entity_forum_topic_type_add_body($type->type);
  }
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
 * right, their type is their topic's type, so this page only explains
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
    '#markup' => '<p>' . t('Replies in %type topics have their <strong>own field collection, separate from the topic\'s fields</strong> - the topic\'s <em>Manage fields</em> tab and the <em>Manage reply fields</em> tab below are two independent sets. For example, a %type topic may carry price and contact fields while its replies carry none, or an offer amount that the topic does not. A reply takes its type from the topic it belongs to, so there is nothing to choose: post a reply in a %type topic and it is a %type reply.', array(
      '%type' => $topic_type->label,
    )) . '</p>',
  );
  if (module_exists('field_ui')) {
    $build['links'] = array(
      '#theme' => 'item_list',
      '#items' => array(
        l(t('Manage reply fields'), $path . '/fields'),
        l(t('Manage reply display'), $path . '/display'),
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

/**
 * Page callback: the Help tab (admin/content/entity-forum/help).
 *
 * A plain in-module help page, Backdrop core has dropped hook_help(), so
 * this is our own page rather than a hook_help() implementation. The body is
 * assembled from four audience helpers (site builders, forum builders,
 * moderators, members). Keeping each audience in its own helper is
 * deliberate: when the page is later split, the moderator and member
 * sections becoming frontend pages with their own access, a helper is
 * lifted whole rather than untangled from a monolithic string.
 *
 * @return array
 *   Render array.
 */
function entity_forum_help_page() {
  $sections = array(
    'site-builders' => array(t('For site builders'), _entity_forum_help_site_builders()),
    'forum-builders' => array(t('For forum builders'), _entity_forum_help_forum_builders()),
    'moderators' => array(t('For moderators'), _entity_forum_help_moderators()),
    'members' => array(t('For members'), _entity_forum_help_members()),
  );

  $build = array();
  backdrop_add_css(backdrop_get_path('module', 'entity_forum') . '/css/entity_forum_admin.css');

  // Table of contents: an in-page jump list to each audience section.
  $toc = array();
  foreach ($sections as $anchor => $info) {
    $toc[] = '<a href="#entity-forum-help-' . $anchor . '">' . check_plain($info[0]) . '</a>';
  }
  $html = '<div class="entity-forum-help">';
  $html .= '<p>' . t('Entity Forum ships primitives, not policy: forums, topics and replies are ordinary entities, so you shape a forum with the fields, views and permissions you already know. This page is grouped by who is doing the work.') . '</p>';
  $html .= '<div class="entity-forum-help-toc"><strong>' . t('On this page:') . '</strong> ' . implode(' &middot; ', $toc) . '</div>';

  foreach ($sections as $anchor => $info) {
    $html .= '<section id="entity-forum-help-' . $anchor . '">';
    $html .= '<h2>' . check_plain($info[0]) . '</h2>';
    $html .= $info[1];
    $html .= '</section>';
  }
  $html .= '</div>';

  $build['help'] = array('#markup' => $html);
  return $build;
}

/**
 * Help content: getting the module installed and setting site-wide policy.
 *
 * Covers the parts a sitedev decides once, requirements, permissions, the
 * per-field access model, and node-attached discussions, including the two
 * traps that have no home in the README: Content Access does nothing here,
 * and core comments must be disabled by hand on a discussion-enabled type.
 *
 * @return string
 *   HTML markup.
 */
function _entity_forum_help_site_builders() {
  $perms = l(t('permissions'), 'admin/config/people/permissions');
  $settings = l(t('Settings tab'), 'admin/content/entity-forum/settings');

  $html = '';
  $html .= '<p>' . t('Requirements: the <em>entity</em>, <em>entity_plus</em>, <em>views</em> and <em>text</em> modules. Entity Forum provides the <code>forums</code>, <code>forum/*</code> and <code>topic/*</code> paths, so it cannot run alongside the contrib <em>Forum</em> or <em>Forum NG</em> modules - installation is blocked while either is enabled.') . '</p>';

  $html .= '<h3>' . t('Setup order') . '</h3>';
  $html .= '<ol>';
  $html .= '<li>' . t('Create your forums (see <em>For forum builders</em> below).') . '</li>';
  $html .= '<li>' . t('Grant @perms - the important ones are listed next.', array('@perms' => $perms)) . '</li>';
  $html .= '<li>' . t('Review the @settings for forum pictures, moderation and node discussions.', array('@settings' => $settings)) . '</li>';
  $html .= '<li>' . t('Optional: add topic types and attach fields.') . '</li>';
  $html .= '</ol>';

  $html .= '<h3>' . t('Permissions') . '</h3>';
  $html .= '<p>' . t('Grant to ordinary members: <em>Create Entity Forum topics</em> and <em>Create Entity Forum replies</em> (each also lets a member edit and delete their own posts). Grant <em>View private Entity Forums</em> to whichever roles may see private forums. Moderators need <em>Moderate Entity Forum</em>, usually combined with <em>Administer Entity Forum topics</em> and <em>replies</em> for full moderator rights. <em>Administer Entity Forum</em> is the forum-builder permission that gates this admin section.') . '</p>';

  $html .= '<h3>' . t('Per-field access, and a trap') . '</h3>';
  $html .= '<p>' . t('To let only some roles add attachments, or to show an offer amount only to the seller and moderators, install the contrib <em>Field Permissions</em> module and set permissions per field. Entity Forum bridges post authorship so the <em>own</em>-content variants work correctly on forum posts.') . '</p>';
  $html .= '<p class="messages warning">' . t('<strong>Content Access does nothing here.</strong> It is a <em>node</em> access module and works through the node access system; forums, topics and replies are entities, not nodes, so it never runs on them. <em>Field Permissions</em> is the module that applies - installing Content Access to gate forum content will silently do nothing.') . '</p>';

  $html .= '<h3>' . t('Node discussions (a comments replacement)') . '</h3>';
  $html .= '<p>' . t('Add a placement on the @settings to give a content type a forum discussion instead of core comments. Choose a scope:', array('@settings' => $settings)) . '</p>';
  $html .= '<ul>';
  $html .= '<li>' . t('<strong>Every item of this type</strong> - a discussion is created for each piece of content automatically.') . '</li>';
  $html .= '<li>' . t('<strong>Only items the author enables</strong> - adds a <em>Forum Discussions</em> checkbox to the content edit form, so a discussion appears only where it is switched on.') . '</li>';
  $html .= '</ul>';
  $html .= '<p class="messages warning">' . t('Either way, <strong>disable core comments on that content type yourself</strong> - the module deliberately does not touch core comment settings, so without this a node would show both.') . '</p>';

  return $html;
}

/**
 * Help content: building forums, topic types and their views.
 *
 * Leads with the topic-type workflow because setting the type on the FORUM
 * is the least discoverable step in the module, posting never asks for it.
 *
 * @return string
 *   HTML markup.
 */
function _entity_forum_help_forum_builders() {
  $forums = l(t('Forums admin'), 'admin/content/entity-forum');
  $types = l(t('Topic types'), 'admin/structure/entity-forum/topic-types');

  $html = '';
  $html .= '<h3>' . t('Forums and sub-forums') . '</h3>';
  $html .= '<p>' . t('Add forums from the @forums with <em>Add forum</em>. A forum with a parent becomes a sub-forum, with post counts and breadcrumbs rolled up automatically. Mark a forum <em>private</em> and its topics, replies and file attachments all inherit that privacy.', array('@forums' => $forums)) . '</p>';
  $html .= '<p>' . t('Setting a forum\'s topic type to <strong>- None -</strong> makes it a <em>container</em>: it groups sub-forums and takes no topics of its own (the phpBB/bbPress "category"). A forum can still hold both sub-forums and topics - being a container is a deliberate choice, never inferred from having sub-forums.') . '</p>';

  $html .= '<h3>' . t('Topic types, and the step everyone misses') . '</h3>';
  $html .= '<p>' . t('Topic types work like content types: each has its own fields, and a forum is set to one - so a Classified Adverts forum can carry a price and photos while the discussion forums stay plain. The workflow, in order:') . '</p>';
  $html .= '<ol>';
  $html .= '<li>' . t('Add a type on @types.', array('@types' => $types)) . '</li>';
  $html .= '<li>' . t('Add its fields on that type\'s <em>Manage fields</em> tab.') . '</li>';
  $html .= '<li>' . t('Add any reply fields on the type\'s <em>Replies</em> tab (a reply takes its type from the topic it is posted in, so a reply to a classified ad can carry, say, an offer amount).') . '</li>';
  $html .= '<li>' . t('<strong>Set the type on the FORUM</strong>, on the forum edit form.') . '</li>';
  $html .= '</ol>';
  $html .= '<p class="messages warning">' . t('<strong>The last step is the one that catches people.</strong> Posting never asks which type to use - the forum decides - so a topic type you have built does nothing until a forum is pointed at it. A forum that wants a different kind of topic gets a sub-forum set to that type. Changing a forum\'s type does not retype topics already in it; they keep the type they were created with.') . '</p>';

  $html .= '<h3>' . t('Views: copy, do not edit') . '</h3>';
  $html .= '<p>' . t('The shipped views are meant to be <strong>copied and amended</strong>, not edited in place. Each forum picks which view display renders its topic list (on the forum edit form), so a classified-ads forum can point at a copied grid view showing an image and price while the discussion forums keep the standard topic table. Clone the <code>entity_forum_topics</code> view, shape the copy, then select that display on the forum.') . '</p>';
  $html .= '<p>' . t('Privacy is enforced on the base tables themselves, so a copied view is safe - but always confirm a new forum view does not expose a private forum before relying on it.') . '</p>';

  return $html;
}

/**
 * Help content: the moderation tools.
 *
 * Written against the frontend moderation page so it lifts cleanly to a
 * frontend help location once the page is split by audience.
 *
 * @return string
 *   HTML markup.
 */
function _entity_forum_help_moderators() {
  $queue = l(t('Forum moderation'), 'forums/moderation');

  $html = '';
  $html .= '<p>' . t('Moderation needs the <em>Moderate Entity Forum</em> permission (combine it with <em>Administer Entity Forum topics</em> and <em>replies</em> to edit and delete any post).') . '</p>';
  $html .= '<h3>' . t('The queue') . '</h3>';
  $html .= '<p>' . t('Posts held for review collect on the @queue page, reachable from the moderation panels shown above the forum lists. Each held post can be <em>approved</em> (published) or <em>kept hidden</em>.', array('@queue' => $queue)) . '</p>';
  $html .= '<h3>' . t('Holds and suspensions') . '</h3>';
  $html .= '<p>' . t('Set a member\'s posting status to hold their future posts for review, or to suspend them from posting altogether. A held member\'s posts still submit - they queue for approval rather than appearing at once.') . '</p>';
  $html .= '<h3>' . t('Reports') . '</h3>';
  $html .= '<p>' . t('Members can report a topic or reply. A reported post stays visible with a moderator-only highlight rather than being hidden automatically - a human decides. Resolve a report from the same moderation tools.') . '</p>';

  return $html;
}

/**
 * Help content: posting, replying and the member-facing features.
 *
 * The most frontend-oriented section; written to move to a members' help
 * page near the forums when the split happens.
 *
 * @return string
 *   HTML markup.
 */
function _entity_forum_help_members() {
  $forums = l(t('Forums'), 'forums');

  $html = '';
  $html .= '<p>' . t('Browse from the @forums index. Posting needs the <em>Create Entity Forum topics</em> and <em>replies</em> permissions.', array('@forums' => $forums)) . '</p>';
  $html .= '<ul>';
  $html .= '<li>' . t('<strong>Start a topic</strong> from a forum\'s <em>New topic</em> link. Any fields the forum\'s topic type carries (a price, a photo) appear on the form.') . '</li>';
  $html .= '<li>' . t('<strong>Reply</strong> from the <em>Reply</em> link on a topic or post. Replies are threaded and paged so a conversation is never split across pages.') . '</li>';
  $html .= '<li>' . t('<strong>Report</strong> a topic or reply with its <em>Report</em> link if it needs a moderator\'s attention.') . '</li>';
  $html .= '<li>' . t('<strong>Forum picture</strong> - set a personal avatar on the <em>Forum</em> tab of the account page; it shows on posts and in topic lists.') . '</li>';
  $html .= '<li>' . t('<strong>Mark as sold / close</strong> - in forums that allow it, the topic author gets a self-service link to close (and reopen) their own topic, e.g. to mark a classified ad sold.') . '</li>';
  $html .= '</ul>';

  return $html;
}
