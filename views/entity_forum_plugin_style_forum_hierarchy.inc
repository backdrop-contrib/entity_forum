<?php
/**
 * @file
 * Views style plugin: render forum rows as an indented hierarchy.
 */

/**
 * Forum hierarchy style plugin.
 *
 * Views has no tree style, which is why the front-end forums index and the
 * admin forum overview each hand-roll the same depth-first walk in PHP. This
 * style adds that one missing piece so either list can become a View: it takes
 * the flat result set on the entity_forum_forum base table, re-orders it
 * depth-first (each forum immediately followed by its descendants), and indents
 * one chosen column by nesting depth — the same visual the two callbacks
 * produce with padding-left.
 *
 * It extends the core Table style rather than reinventing it, so a site builder
 * keeps every table feature (which fields are columns, headers, per-column
 * classes, alignment, responsive) and this class only contributes the ordering
 * and the indent. Sibling order comes from the view's own Sort criteria (e.g.
 * weight, then title) — the reorder never sorts, it only nests.
 *
 * @see entity_forum_views_plugins()
 * @ingroup views_style_plugins
 */
class entity_forum_plugin_style_forum_hierarchy extends views_plugin_style_table {

  /**
   * Query alias holding each row's forum ID (the base field).
   *
   * @var string
   */
  protected $id_alias;

  /**
   * Query alias holding each row's parent forum ID.
   *
   * @var string
   */
  protected $parent_alias;

  /**
   * Nesting depth per row, keyed by the row's final result index (0 = root).
   *
   * @var int[]
   */
  protected $depths = array();

  /**
   * Guards render_fields() so the indent is applied to the cache only once.
   *
   * @var bool
   */
  protected $indent_applied = FALSE;

  /**
   * Adds the indent-column and indent-width options on top of the table style.
   *
   * @return array
   *   The option definitions.
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['indent_field'] = array('default' => '');
    $options['indent_width'] = array('default' => 24);
    return $options;
  }

  /**
   * Adds the "indent this column" and indent-width controls to the style form.
   */
  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);

    $field_names = $this->display->handler->get_field_labels();
    $form['indent_field'] = array(
      '#type' => 'select',
      '#title' => t('Indent this column'),
      '#options' => $field_names,
      '#default_value' => $this->options['indent_field'],
      '#description' => t('The column indented to show each forum beneath its parent — usually the forum title. Sibling order follows the display\'s Sort criteria (e.g. weight, then title); do not enable click-sorting on a hierarchy, as it fights the nesting.'),
      '#weight' => -6,
    );
    $form['indent_width'] = array(
      '#type' => 'number',
      '#title' => t('Indent width'),
      '#field_suffix' => t('pixels per level'),
      '#default_value' => $this->options['indent_width'],
      '#min' => 0,
      '#max' => 200,
      '#weight' => -5,
    );
  }

  /**
   * Ensures forum_id and parent_id are in the query so the tree can be built.
   *
   * Read straight from the base table, not required as visible fields: the
   * hierarchy must work even when parent_id is not a column the builder chose
   * to show.
   */
  function query() {
    parent::query();
    $base = $this->view->base_table;
    $this->id_alias = $this->view->query->add_field($base, $this->view->base_field);
    $this->parent_alias = $this->view->query->add_field($base, 'parent_id');
  }

  /**
   * Keeps the view's own sort — a hierarchy is ordered by nesting, not clicks.
   *
   * Returning TRUE tells the builder NOT to let click-sorting replace the
   * configured sorts, so sibling order (weight, title) is stable.
   *
   * @return bool
   *   Always TRUE.
   */
  function build_sort() {
    return TRUE;
  }

  /**
   * No-op: suppresses the table style's click-sort, which would break nesting.
   */
  function build_sort_post() {
  }

  /**
   * Re-orders the result depth-first, then renders through the table style.
   *
   * @return string
   *   The rendered table.
   */
  function render() {
    $this->reorder_into_tree();
    return parent::render();
  }

  /**
   * Rebuilds $view->result in depth-first order and records each row's depth.
   *
   * Groups rows by parent, preserving the query's order for siblings, then
   * walks the tree from the roots. A row whose parent is not in this result set
   * (a private forum filtered out, a paged slice) is treated as a root so it is
   * never dropped; a cyclic parent chain is broken by the emitted guard, and
   * any row still unreached is appended at depth 0.
   */
  protected function reorder_into_tree() {
    $result = $this->view->result;
    if (empty($result)) {
      return;
    }

    $present = array();
    foreach ($result as $row) {
      $present[$row->{$this->id_alias}] = TRUE;
    }

    // Siblings keyed by parent id, in the query's (sorted) order.
    $children = array();
    foreach ($result as $row) {
      $parent = $row->{$this->parent_alias};
      $root = (!empty($parent) && isset($present[$parent])) ? $parent : 0;
      $children[$root][] = $row;
    }

    $ordered = array();
    $depths = array();
    $emitted = array();
    $this->walk_tree($children, 0, 0, $ordered, $depths, $emitted);

    // Anything unreached (a cycle) still gets shown, at the top level.
    foreach ($result as $row) {
      if (empty($emitted[$row->{$this->id_alias}])) {
        $emitted[$row->{$this->id_alias}] = TRUE;
        $depths[count($ordered)] = 0;
        $ordered[] = $row;
      }
    }

    $this->view->result = $ordered;
    $this->depths = $depths;
  }

  /**
   * Depth-first walk emitting each forum immediately before its descendants.
   *
   * @param array $children
   *   Rows grouped by parent id.
   * @param int $parent_id
   *   Parent whose children are emitted (0 for roots).
   * @param int $depth
   *   Current nesting depth.
   * @param array $ordered
   *   Rows in tree order, built by reference (0-indexed sequentially).
   * @param array $depths
   *   depth keyed by the row's index in $ordered, built by reference.
   * @param array $emitted
   *   forum_id => TRUE for rows already emitted, by reference — breaks cycles.
   */
  protected function walk_tree(array $children, $parent_id, $depth, array &$ordered, array &$depths, array &$emitted) {
    if (empty($children[$parent_id])) {
      return;
    }
    foreach ($children[$parent_id] as $row) {
      $id = $row->{$this->id_alias};
      if (!empty($emitted[$id])) {
        // A cyclic parent chain would otherwise loop forever.
        continue;
      }
      $emitted[$id] = TRUE;
      $depths[count($ordered)] = $depth;
      $ordered[] = $row;
      $this->walk_tree($children, $id, $depth + 1, $ordered, $depths, $emitted);
    }
  }

  /**
   * Renders fields, then indents the chosen column by each row's depth.
   *
   * Wraps the rendered value of the indent column in a padded span so nested
   * forums sit under their parent, matching the front-end index / admin
   * overview. Applied to the memoized field cache exactly once.
   *
   * @param array $result
   *   The result rows (already in tree order).
   *
   * @return array
   *   The rendered field cache.
   */
  function render_fields($result) {
    $rendered = parent::render_fields($result);
    if ($rendered === NULL || $this->indent_applied) {
      return $rendered;
    }
    $this->indent_applied = TRUE;

    $field = $this->options['indent_field'];
    $width = (int) $this->options['indent_width'];
    if ($field === '' || $width <= 0 || empty($this->view->field[$field])) {
      return $this->rendered_fields;
    }

    foreach ($this->rendered_fields as $index => &$row) {
      $depth = isset($this->depths[$index]) ? $this->depths[$index] : 0;
      if ($depth > 0 && isset($row[$field]) && $row[$field] !== '') {
        $row[$field] = '<span class="entity-forum-hierarchy-indent" style="padding-left: '
          . ($depth * $width) . 'px;">' . $row[$field] . '</span>';
      }
    }
    unset($row);

    return $this->rendered_fields;
  }

}
