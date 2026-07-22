<?php
/**
 * @file
 * Views field handler for the forum-index "name" cell.
 */

/**
 * Renders a forum's identity cell: linked title + "(private)" marker +
 * formatted description.
 *
 * The forums index shows the forum name and its description as one unit, and
 * the Forum hierarchy style indents a single field's value as a whole, so the
 * title, the private marker and the description have to travel together in one
 * field, or a nested forum's description would sit un-indented at the cell's
 * left edge while its title was pushed right. The style supplies the
 * padding-left indent around this cell.
 *
 * Topics and Last activity stay separate, editable Views columns; only the
 * identity cell is composite, because only it needs to indent as a unit.
 */
class entity_forum_handler_field_forum_link extends views_handler_field {

  /**
   * Query aliases for the columns the cell needs, keyed by column name.
   *
   * @var string[]
   */
  protected $parts = array();

  /**
   * Adds forum_id (the real field, for the link) and the cell's columns.
   */
  function query() {
    $this->ensure_my_table();
    $this->field_alias = $this->query->add_field($this->table_alias, $this->real_field);
    foreach (array('title', 'visibility', 'description', 'description_format', 'icon_fid', 'icon_style') as $column) {
      $this->parts[$column] = $this->query->add_field($this->table_alias, $column);
    }
    $this->add_additional_fields();
  }

  /**
   * Reads a fetched column off the result row.
   *
   * @param object $values
   *   The result row.
   * @param string $column
   *   The column name added in query().
   *
   * @return string
   *   The column value, or '' when absent.
   */
  protected function part($values, $column) {
    $alias = $this->parts[$column];
    return isset($values->$alias) ? $values->$alias : '';
  }

  /**
   * Renders the forum name cell.
   *
   * @param object $values
   *   The result row.
   *
   * @return string
   *   The cell markup, or '' when the row has no forum ID.
   */
  function render($values) {
    $forum_id = $this->get_value($values);
    if (!$forum_id) {
      return '';
    }

    $title = l($this->part($values, 'title'), 'forum/' . $forum_id);
    if ($this->part($values, 'visibility') === 'private') {
      $title .= ' <span class="entity-forum-private">' . t('(private)') . '</span>';
    }

    // The icon renders at the forum's own image style (icon_style), so a
    // top-level forum can carry a large icon here too.
    $icon = entity_forum_format_forum_icon($this->part($values, 'icon_fid'), $this->part($values, 'icon_style'));
    if ($icon !== '') {
      $icon = '<span class="entity-forum-icon">' . $icon . '</span>';
    }

    // Title + description share their own column beside the icon (a flex row),
    // so a tall icon never makes the description wrap underneath it.
    $text = $title;
    $description = $this->part($values, 'description');
    if ($description !== '') {
      $format = $this->part($values, 'description_format');
      $format = $format !== '' ? $format : filter_default_format();
      $text .= '<div class="entity-forum-description">' . check_markup($description, $format) . '</div>';
    }

    return '<div class="entity-forum-title">' . $icon . '<div class="entity-forum-title-text">' . $text . '</div></div>';
  }

}
