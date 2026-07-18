<?php
/**
 * @file
 * Views field handler that maps a raw column value to a human label.
 */

/**
 * Renders a stored value (a bundle machine name, a listing state, …) as its
 * human-readable label.
 *
 * Backdrop stores these as plain strings, so a default Views field prints the
 * machine name ("admin_only", "entity_forum_topic"). This handler swaps in the
 * label from a map supplied in the Views field definition, keeping the raw
 * value queryable/sortable while the table reads naturally. Two ways to supply
 * the map, matching how entity_forum_handler_field_forum_count takes its
 * 'count key':
 * - 'labels callback': a callable returning value => label (e.g.
 *   entity_forum_topic_type_labels), resolved at render time so newly added
 *   types appear without touching the view.
 * - 'value labels': an inline value => label array, for a fixed set of states.
 * An unmapped value falls back to the raw string, so nothing ever renders
 * blank.
 */
class entity_forum_handler_field_value_label extends views_handler_field {

  /**
   * Builds the value => label map from the field definition.
   *
   * @return array
   *   Map of stored value to human label; empty when none is configured.
   */
  protected function labelMap() {
    if (!empty($this->definition['labels callback']) && is_callable($this->definition['labels callback'])) {
      return (array) call_user_func($this->definition['labels callback']);
    }
    if (!empty($this->definition['value labels'])) {
      return (array) $this->definition['value labels'];
    }
    return array();
  }

  /**
   * Renders the mapped label, falling back to the sanitized raw value.
   */
  function render($values) {
    $value = $this->get_value($values);
    if ($value === NULL || $value === '') {
      return '';
    }
    $labels = $this->labelMap();
    if (isset($labels[$value])) {
      // Labels come from config/code, not user input, but t()/config strings
      // may legitimately contain markup callers expect rendered — none here
      // do, so sanitize for safety.
      return check_plain($labels[$value]);
    }
    return check_plain($value);
  }

}
