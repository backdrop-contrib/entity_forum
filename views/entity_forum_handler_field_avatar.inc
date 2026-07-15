<?php
/**
 * @file
 * Views field handler for the forum author picture.
 */

/**
 * Renders a post author's forum picture from the row's author_uid.
 *
 * The picture is module-owned account data (see
 * entity_forum_user_avatar_fid()), so a plain user relationship cannot
 * reach it. Deleted and unknown authors (uid <= 0) render nothing.
 */
class entity_forum_handler_field_avatar extends views_handler_field {

  /**
   * Defines the image style option.
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['image_style'] = array('default' => 'entity_forum_avatar_small');
    return $options;
  }

  /**
   * Adds the image style selector to the field settings form.
   */
  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $styles = array();
    foreach (image_styles() as $name => $style) {
      $styles[$name] = isset($style['label']) ? $style['label'] : $name;
    }
    $form['image_style'] = array(
      '#type' => 'select',
      '#title' => t('Image style'),
      '#options' => $styles,
      '#default_value' => $this->options['image_style'],
    );
  }

  /**
   * Renders the author's picture, or nothing.
   */
  function render($values) {
    $uid = (int) $this->get_value($values);
    return entity_forum_format_avatar($uid, $this->options['image_style']);
  }

}
