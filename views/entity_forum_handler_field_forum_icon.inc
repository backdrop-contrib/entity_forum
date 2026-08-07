<?php
/**
 * @file
 * Views field handler for the forum icon.
 */

/**
 * Renders a forum's icon from the row's icon_fid.
 *
 * The forums index carries its icon inside the composite identity cell
 * (entity_forum_handler_field_forum_link) because the hierarchy style has to
 * indent title, description and icon as one unit. Everywhere else the icon is
 * wanted on its own — a grid of classified ads, a "browse the forums" block —
 * and there it needs to be a field a site builder can place, order and style
 * like any other, including through a topic → forum relationship.
 *
 * A plain image field handler cannot do the job: the icon is a base column, not
 * a Field API image field, a forum without one falls back to the module's
 * shipped default SVG, and SVGs cannot pass through an image style. All three
 * are settled by entity_forum_format_forum_icon().
 */
class entity_forum_handler_field_forum_icon extends views_handler_field {

  /**
   * Query aliases for the extra columns the icon needs, keyed by column name.
   *
   * @var string[]
   */
  protected $parts = array();

  /**
   * Defines the image style option.
   *
   * The default is the empty string, meaning "whatever this forum is set to"
   * — the per-forum icon_style. A view that wants one consistent size
   * regardless (a grid of equal cells) overrides it here.
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['image_style'] = array('default' => '');
    return $options;
  }

  /**
   * Adds the image style selector to the field settings form.
   */
  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $styles = array('' => t("- Each forum's own setting -"));
    foreach (image_styles() as $name => $style) {
      $styles[$name] = isset($style['label']) ? $style['label'] : $name;
    }
    $form['image_style'] = array(
      '#type' => 'select',
      '#title' => t('Image style'),
      '#description' => t("The size the icon renders at. Leave as each forum's own setting to respect the style chosen on the forum edit form; pick a style to force one size throughout this view. SVG icons, including the shipped default, are sized to the style rather than processed through it."),
      '#options' => $styles,
      '#default_value' => $this->options['image_style'],
    );
  }

  /**
   * Adds icon_fid (the real field) plus the forum's own style and title.
   */
  function query() {
    $this->ensure_my_table();
    $this->field_alias = $this->query->add_field($this->table_alias, $this->real_field);
    foreach (array('icon_style', 'title') as $column) {
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
   * Renders the forum's icon.
   *
   * @param object $values
   *   The result row.
   *
   * @return string
   *   Image markup. Never empty for a real forum row — a forum without an
   *   uploaded icon gets the shipped default — but a row whose relationship
   *   found no forum renders nothing.
   */
  function render($values) {
    // A left-joined relationship with no matching forum leaves every column
    // NULL, including the title; the fallback icon would otherwise be drawn
    // for a forum that isn't there.
    if ($this->part($values, 'title') === '') {
      return '';
    }
    $style = $this->options['image_style'] !== ''
      ? $this->options['image_style']
      : $this->part($values, 'icon_style');

    return entity_forum_format_forum_icon($this->get_value($values), $style, $this->part($values, 'title'));
  }

}
