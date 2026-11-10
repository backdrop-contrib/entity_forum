<?php
/**
 * @file
 * Views filter handler: search topics by words in their text.
 */

/**
 * Keeps the topics whose title, opening post or replies contain every word.
 *
 * Each word must appear somewhere in the topic: in its title, in its own body,
 * or (optionally) in the body of one of its replies. Matching a reply lists the
 * topic once, as a forum's topic list does, rather than the reply itself.
 *
 * The replies searched are the ones the viewer could read on the topic page:
 * published ones only, unless the viewer moderates. Restricted topic types,
 * whose full text the viewer may not see, are matched on their title alone,
 * so a search cannot reveal what the hidden part of a topic says. Which topics
 * appear at all is left to entity_forum_views_query_alter(), as for every
 * other topic view (private forums, held topics).
 *
 * Text matching is a plain "contains" on the stored body, markup included, so
 * very short words are ignored: they would match HTML tags and attributes.
 */
class entity_forum_handler_filter_search extends views_handler_filter {

  /**
   * There is one way to match, so the operator is not offered.
   *
   * @var bool
   */
  public $no_operator = TRUE;

  /**
   * {@inheritdoc}
   */
  public function option_definition() { // @codingStandardsIgnoreLine, Views override compliant
    $options = parent::option_definition();
    $options['replies'] = array('default' => TRUE, 'bool' => TRUE);
    return $options;
  }

  /**
   * {@inheritdoc}
   */
  public function options_form(&$form, &$form_state) { // @codingStandardsIgnoreLine, Views override compliant
    parent::options_form($form, $form_state);
    $form['replies'] = array(
      '#type' => 'checkbox',
      '#title' => t('Also search the replies'),
      '#description' => t('A topic matches when any of its published replies contains the words. Without this, only the title and the opening post are searched.'),
      '#default_value' => !empty($this->options['replies']),
    );
  }

  /**
   * {@inheritdoc}
   */
  public function value_form(&$form, &$form_state) { // @codingStandardsIgnoreLine, Views override compliant
    $form['value'] = array(
      '#type' => 'textfield',
      '#title' => t('Search'),
      '#size' => 30,
      '#maxlength' => 128,
      '#default_value' => $this->value,
    );
    if (!empty($form_state['exposed'])) {
      $identifier = $this->options['expose']['identifier'];
      if (!isset($form_state['input'][$identifier])) {
        $form_state['input'][$identifier] = $this->value;
      }
    }
  }

  /**
   * {@inheritdoc}
   */
  public function admin_summary() { // @codingStandardsIgnoreLine, Views override compliant
    if ($this->is_a_group()) {
      return t('grouped');
    }
    $scope = !empty($this->options['replies']) ? t('topics and replies') : t('topics');
    if (!empty($this->options['exposed'])) {
      return t('exposed, @scope', array('@scope' => $scope));
    }
    return t('"@words" in @scope', array('@words' => $this->value, '@scope' => $scope));
  }

  /**
   * {@inheritdoc}
   */
  public function query() {
    // An exposed value arrives wrapped in an array, a configured one as text.
    $text = is_array($this->value) ? (string) reset($this->value) : (string) $this->value;
    $words = entity_forum_search_words($text);
    if (!$words) {
      // Something was typed, but only words too short to search on: match
      // nothing, rather than drop the condition and list every topic.
      if (trim($text) !== '') {
        $this->query->add_where_expression($this->options['group'], '1 = 0');
      }
      return;
    }
    $this->ensure_my_table();

    $field = field_info_field('entity_forum_body');
    $body_table = $field ? _field_sql_storage_tablename($field) : NULL;
    $body_column = $field ? _field_sql_storage_columnname('entity_forum_body', 'value') : NULL;
    $restricted = entity_forum_restricted_topic_types();
    $moderator = entity_forum_user_is_moderator();

    foreach (array_values($words) as $i => $word) {
      $like = '%' . db_like($word) . '%';
      $match = db_or()->condition($this->table_alias . '.title', $like, 'LIKE');

      if ($body_table) {
        $topics = db_select($body_table, 'efs_tb' . $i);
        $topics->fields('efs_tb' . $i, array('entity_id'));
        $topics->condition('efs_tb' . $i . '.entity_type', 'entity_forum_topic');
        $topics->condition('efs_tb' . $i . '.deleted', 0);
        $topics->condition('efs_tb' . $i . '.' . $body_column, $like, 'LIKE');
        if ($restricted) {
          $topics->condition('efs_tb' . $i . '.bundle', $restricted, 'NOT IN');
        }
        $match->condition($this->table_alias . '.topic_id', $topics, 'IN');

        if (!empty($this->options['replies'])) {
          $replies = db_select('entity_forum_reply', 'efs_r' . $i);
          $replies->fields('efs_r' . $i, array('topic_id'));
          $replies->join($body_table, 'efs_rb' . $i, "efs_rb$i.entity_id = efs_r$i.reply_id AND efs_rb$i.entity_type = 'entity_forum_reply' AND efs_rb$i.deleted = 0");
          $replies->condition('efs_rb' . $i . '.' . $body_column, $like, 'LIKE');
          if (!$moderator) {
            $replies->condition('efs_r' . $i . '.status', 1);
          }
          if ($restricted) {
            $replies->condition('efs_r' . $i . '.type', $restricted, 'NOT IN');
          }
          $match->condition($this->table_alias . '.topic_id', $replies, 'IN');
        }
      }

      $this->query->add_where($this->options['group'], $match);
    }
  }

}
