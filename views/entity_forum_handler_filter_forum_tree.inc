<?php
/**
 * @file
 * Views filter handler: a forum and everything beneath it.
 */

/**
 * Filters topics (or replies) to the chosen forums and their sub-forums.
 *
 * Choosing a container forum, which holds no topics of its own, means the
 * forums inside it, so each chosen forum is expanded to its whole sub-tree.
 * The options are the forum tree in index order, indented, without the
 * private forums the viewer cannot see.
 */
class entity_forum_handler_filter_forum_tree extends views_handler_filter_in_operator {

  /**
   * {@inheritdoc}
   */
  public function get_value_options() { // @codingStandardsIgnoreLine, Views override compliant
    if (isset($this->value_options)) {
      return $this->value_options;
    }
    module_load_include('inc', 'entity_forum', 'entity_forum.admin');
    $options = entity_forum_forum_options(NULL, TRUE);
    if (!entity_forum_user_sees_private()) {
      foreach (entity_forum_private_forum_ids() as $forum_id) {
        unset($options[$forum_id]);
      }
    }
    $this->value_options = $options;
    return $options;
  }

  /**
   * {@inheritdoc}
   */
  public function query() {
    $chosen = array_filter(array_map('intval', (array) $this->value));
    if (!$chosen) {
      return;
    }
    $forum_ids = array();
    foreach ($chosen as $forum_id) {
      $forum_ids = array_merge($forum_ids, entity_forum_forum_subtree_ids($forum_id));
    }
    $this->ensure_my_table();
    $operator = $this->operator === 'not in' ? 'NOT IN' : 'IN';
    $this->query->add_where($this->options['group'], $this->table_alias . '.' . $this->real_field, array_values(array_unique($forum_ids)), $operator);
  }

}
