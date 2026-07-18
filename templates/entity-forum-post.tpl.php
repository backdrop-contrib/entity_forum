<?php
/**
 * @file
 * Default theme implementation for a single forum post.
 *
 * A "post" is a topic's opening post or a reply. The threading, paging and
 * per-viewer moderation decisions are made in code; this template only lays out
 * the already-rendered pieces, so a theme can restyle the post shell by copying
 * this file into its own directory (optionally renamed per suggestion).
 *
 * Available variables:
 * - $classes: Sanitized, space-separated wrapper classes — 'entity-forum-post',
 *   the kind class ('entity-forum-post-topic' or '-reply'), the topic-type class
 *   ('entity-forum-post-type-BUNDLE'), plus any state classes (e.g.
 *   'entity-forum-reported' on a post a moderator sees flagged).
 * - $html_id: Sanitized anchor id for permalinks (e.g. "topic-42", "reply-12").
 * - $prefix: Extra markup above the byline (already sanitized) — e.g. the
 *   "In reply to #N" context line or a moderator's reported-content badge.
 * - $avatar: Rendered author forum picture (may be empty).
 * - $byline: Rendered author name + timestamp line.
 * - $body_html: Rendered, sanitized post body. Empty for a node-attached
 *   discussion topic (the node is the opening post).
 * - $fields: Rendered attached fields (images, price, etc.) in Manage Display
 *   order (may be empty).
 * - $signature: Rendered user signature (may be empty; gated site-wide).
 * - $links: Rendered reply / report / edit / delete links (may be empty).
 * - $kind: 'topic' or 'reply'.
 * - $bundle: The topic-type machine name (the default type is
 *   'entity_forum_topic'); '' if unknown.
 *
 * Also available (raw, for a themer who wants to rebuild a piece):
 * - $uid, $created, $author_name, $entity_type, $extra_classes.
 *
 * Template suggestions (drop a copy of this file, renamed, into your theme):
 * - entity-forum-post--topic.tpl.php / entity-forum-post--reply.tpl.php
 * - entity-forum-post--topic--BUNDLE.tpl.php (e.g. --topic--classified)
 * - entity-forum-post--reply--BUNDLE.tpl.php
 * The most specific existing template wins. For "all posts of a topic type
 * regardless of kind", the .entity-forum-post-type-BUNDLE class is simpler than
 * a template.
 *
 * @see template_preprocess_entity_forum_post()
 * @see _entity_forum_render_post()
 *
 * @ingroup themeable
 */
?>
<div class="<?php print $classes; ?>" id="<?php print $html_id; ?>">
  <?php print $prefix; ?>
  <?php if ($avatar): ?>
    <div class="entity-forum-avatar"><?php print $avatar; ?></div>
  <?php endif; ?>
  <div class="entity-forum-byline"><?php print $byline; ?></div>
  <?php if ($body_html !== ''): ?>
    <div class="entity-forum-content"><?php print $body_html; ?></div>
  <?php endif; ?>
  <?php print $fields; ?>
  <?php print $signature; ?>
  <?php if ($links): ?>
    <div class="entity-forum-post-links"><?php print $links; ?></div>
  <?php endif; ?>
</div>
