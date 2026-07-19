<?php
/**
 * @file
 * Views area handler: the moderator queue panel shown above a forum list.
 */

/**
 * Renders the moderation panel in a view's header, footer or empty area.
 *
 * Like entity_forum_handler_area_post_link, this exists for the per-viewer
 * decision Views cannot make itself: the panel is shown ONLY to moderators, and
 * only when something is waiting, a display's access setting would gate the
 * whole view, and a "Global: Text area" has no idea who is looking. It defers
 * entirely to _entity_forum_moderation_panel(), which checks
 * entity_forum_user_is_moderator() and the pending counts, so every rule stays
 * in one place and a copy of the view keeps the panel with no configuration.
 *
 * Rendered regardless of whether the view produced results: a moderator on a
 * site whose forums are all empty still wants to see items awaiting review.
 */
class entity_forum_handler_area_moderation_panel extends views_handler_area {

  /**
   * Implements views_handler_area::render().
   *
   * @param bool $empty
   *   Whether the view produced no results (unused, the panel is relevant to
   *   moderators either way).
   *
   * @return string
   *   The rendered panel, or '' for non-moderators / an empty queue.
   */
  function render($empty = FALSE) {
    // _entity_forum_moderation_panel() lives in the pages include, which is not
    // loaded just because a view is rendering.
    module_load_include('inc', 'entity_forum', 'entity_forum.pages');
    $panel = _entity_forum_moderation_panel();
    return $panel ? backdrop_render($panel) : '';
  }

}
