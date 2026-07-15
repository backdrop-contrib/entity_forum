<?php
/**
 * @file
 * Hooks provided by the Entity Forum module.
 *
 * The frontend forms are ordinary Form API forms, so they need no custom
 * hooks — alter them with hook_form_FORM_ID_alter():
 * - entity_forum_topic_frontend_form (the "New topic" form)
 * - entity_forum_reply_frontend_form (the reply form on topic pages)
 * - entity_forum_posting_name_form (the account Forum tab)
 *
 * The topic list on forum pages is the entity_forum_topics view — change
 * it in the Views UI (admin/structure/views) or with the Views hooks.
 */

/**
 * Alter the forums index page ("forums").
 *
 * @param array $build
 *   The page render array: 'forums' (the index table).
 */
function hook_entity_forum_forums_page_alter(&$build) {
  $build['notice'] = array(
    '#markup' => '<p>' . t('Welcome to our forums!') . '</p>',
    '#weight' => -10,
  );
}

/**
 * Alter a forum page ("forum/N") before it is rendered.
 *
 * @param array $build
 *   The page render array: 'description', 'subforums', 'new_topic'
 *   (the Post new topic link) and 'topics' (the embedded
 *   entity_forum_topics view).
 * @param EntityForumForum $forum
 *   The forum being viewed.
 */
function hook_entity_forum_forum_page_alter(&$build, $forum) {
  if ($forum->visibility === 'private') {
    unset($build['description']);
  }
}

/**
 * Alter a topic page ("topic/N") before it is rendered.
 *
 * @param array $build
 *   The page render array: 'topic' (the opening post), 'replies',
 *   'pager', and one of 'reply_form', 'closed' or 'login'.
 * @param EntityForumTopic $topic
 *   The topic being viewed.
 */
function hook_entity_forum_topic_page_alter(&$build, $topic) {
  if ($topic->closed) {
    $build['closed']['#markup'] .= '<p>' . t('Contact a moderator to reopen it.') . '</p>';
  }
}
