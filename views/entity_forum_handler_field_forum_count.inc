<?php
/**
 * @file
 * Views field handler for aggregate forum counts.
 */

/**
 * Renders a per-forum aggregate count (topics, posts, replies) or last
 * activity, rolled up to include sub-forums.
 *
 * All values come from the cached counts map (entity_forum_forum_counts()),
 * so a forums-list View with any number of rows costs the two grouped
 * queries that build the map once, never a count query per row. Which
 * value to show is set by the 'count key' in the Views field definition;
 * 'is date' marks the last-activity field so it offers a date format.
 */
class entity_forum_handler_field_forum_count extends views_handler_field {

  /**
   * Exposes a date-format option for the last-activity field.
   */
  function option_definition() {
    $options = parent::option_definition();
    if (!empty($this->definition['is date'])) {
      $options['date_format'] = array('default' => 'short');
    }
    return $options;
  }

  /**
   * Adds the date-format selector for the last-activity field.
   */
  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    if (!empty($this->definition['is date'])) {
      $formats = array();
      foreach (system_get_date_formats() as $type => $info) {
        $formats[$type] = format_date(REQUEST_TIME, $type);
      }
      $form['date_format'] = array(
        '#type' => 'select',
        '#title' => t('Date format'),
        '#options' => $formats,
        '#default_value' => $this->options['date_format'],
      );
    }
  }

  /**
   * Reads the forum ID for this row (the handler's real field).
   */
  function query() {
    $this->ensure_my_table();
    $this->field_alias = $this->query->add_field($this->table_alias, $this->real_field);
    $this->add_additional_fields();
  }

  /**
   * Renders the requested count value from the cached counts map.
   */
  function render($values) {
    $forum_id = $this->get_value($values);
    if ($forum_id === NULL) {
      return '';
    }
    $counts = entity_forum_forum_counts();
    if (!isset($counts[$forum_id])) {
      return !empty($this->definition['is date']) ? '' : '0';
    }
    $key = $this->definition['count key'];
    $value = isset($counts[$forum_id][$key]) ? $counts[$forum_id][$key] : 0;

    if (!empty($this->definition['is date'])) {
      return $value ? format_date($value, $this->options['date_format']) : t('Never');
    }
    return (string) $value;
  }

}
