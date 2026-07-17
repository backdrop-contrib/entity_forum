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
 * The topic list on forum pages is a view display, chosen per forum on the
 * forum form and defaulting to the shipped entity_forum_topics view — change
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
 *   (the Post new topic link) and 'topics' (the topic list view display this
 *   forum uses — entity_forum_forum_topic_list_view() resolves which).
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

/**
 * Decide whether a new frontend post is held for moderator review.
 *
 * Fired by entity_forum_post_hold() when a member submits a topic or
 * reply through the frontend forms, before the entity is saved. Set
 * $hold['hold'] to TRUE to place the post in the moderation queue: the
 * member sees a "will appear once a moderator has approved it" message,
 * the post is hidden by the standard machinery (admin_only listing for
 * topics, unpublished for replies) and carries $hold['reason'] in its
 * pending_reason column, shown to moderators as the "Held by" value.
 *
 * Entity Forum's own rule (the member's 'review' posting status) runs
 * first; this alter can add rules — an AI moderation verdict, a
 * probation period for new members, per-forum rules — or override the
 * built-in decision. Keep the check fast: it runs synchronously in the
 * posting request. For slow analysis (e.g. a remote AI service), hold
 * the post here and approve it later from your own code — publishing a
 * held post through a normal entity save releases the hold
 * automatically (the pending flag and reason are cleared in presave).
 *
 * @param array $hold
 *   - hold: (bool) TRUE to hold the post for review.
 *   - reason: (string) short machine reason, stored in pending_reason
 *     (max 64 chars). Use a recognisable prefix, e.g. 'ai:spam 0.92'.
 * @param array $context
 *   - entity: the unsaved topic or reply, content already set.
 *   - entity_type: 'entity_forum_topic' or 'entity_forum_reply'.
 *   - account: the posting user account.
 */
function hook_entity_forum_post_hold_alter(&$hold, $context) {
  // Example: hold everything a member posts in their first 14 days.
  $account = $context['account'];
  if (!$hold['hold'] && REQUEST_TIME - $account->created < 14 * 86400) {
    $hold['hold'] = TRUE;
    $hold['reason'] = 'probation';
  }
}
