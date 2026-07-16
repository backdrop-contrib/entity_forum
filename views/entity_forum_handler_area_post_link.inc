<?php

/**
 * @file
 * Views area handler: the "Post new topic" link for a forum.
 */

/**
 * Renders a forum's post link in a view's header, footer or empty area.
 *
 * The point of this handler is the access check. Views' own "Global: Text
 * area" has no idea who is looking, and a display's access setting gates the
 * WHOLE view, not one region — so a hand-written link in a text area would
 * show to anonymous visitors and to members whose posting is suspended.
 *
 * This defers entirely to entity_forum_topic_create_access(), so the link
 * inherits every rule already in force — the create permission, the forum
 * being published and viewable, and the member's posting status — and keeps
 * inheriting them if those rules change. Nothing here re-states them.
 *
 * The forum comes from the view's forum_id argument, which is what makes the
 * handler work on a clone: point a copied display at another forum and the
 * link follows, with no configuration.
 */
class entity_forum_handler_area_post_link extends views_handler_area {

  /**
   * Implements views_handler::option_definition().
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['link_text'] = array('default' => '', 'translatable' => TRUE);
    $options['show_login'] = array('default' => FALSE, 'bool' => TRUE);
    return $options;
  }

  /**
   * Implements views_handler::options_form().
   */
  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);

    $form['link_text'] = array(
      '#type' => 'textfield',
      '#title' => t('Link text'),
      '#default_value' => $this->options['link_text'],
      '#description' => t('Leave empty for the default, "Post new topic".'),
    );
    $form['show_login'] = array(
      '#type' => 'checkbox',
      '#title' => t('Show a log-in prompt to anonymous visitors'),
      '#default_value' => $this->options['show_login'],
      '#description' => t('When unticked, visitors who cannot post simply see nothing here.'),
    );
    $form['help'] = array(
      '#markup' => '<div class="description">' . t('Shown only to viewers who may actually post in this forum — the same check the forum page itself uses. The forum is taken from this display\'s forum ID argument, so a copy of this view pointed at another forum needs no changes.') . '</div>',
    );
  }

  /**
   * Implements views_handler_area::render().
   *
   * @param bool $empty
   *   Whether the view produced no results.
   *
   * @return string
   *   The rendered link, a log-in prompt, or '' when the viewer cannot post.
   */
  function render($empty = FALSE) {
    if ($empty && empty($this->options['empty'])) {
      return '';
    }

    $forum = $this->get_forum();
    if (!$forum) {
      return '';
    }

    if (entity_forum_topic_create_access($forum)) {
      $text = $this->options['link_text'] !== '' ? $this->options['link_text'] : t('Post new topic');
      return '<div class="entity-forum-actions">' . l($text, 'forum/' . $forum->id() . '/new-topic', array(
        'attributes' => array('class' => array('entity-forum-new-topic-link', 'button')),
      )) . '</div>';
    }

    // Nothing to offer, unless the site wants anonymous visitors nudged.
    if (!empty($this->options['show_login']) && user_is_anonymous()) {
      return '<div class="entity-forum-login">' . t('<a href="@login">Log in</a> to post a new topic.', array(
        '@login' => url('user/login', array('query' => backdrop_get_destination())),
      )) . '</div>';
    }

    return '';
  }

  /**
   * Loads the forum this display is showing.
   *
   * Reads the view's argument rather than the current path, so the handler
   * works wherever the view is rendered — embedded in the forum page, a page
   * display, or a preview in the Views UI.
   *
   * Matched on the argument's FIELD (forum_id) rather than its key, because
   * a second argument on the same column is keyed forum_id_1. Falls back to
   * the raw argument list, since $argument->argument is only populated once
   * the view has executed and an area can be asked to render before that.
   *
   * @return EntityForumForum|false
   *   The forum, or FALSE when no forum argument is available — in which
   *   case there is no single forum to post in and the area renders nothing.
   */
  protected function get_forum() {
    $forum_id = 0;

    if (!empty($this->view->argument)) {
      foreach ($this->view->argument as $argument) {
        if (isset($argument->field) && $argument->field === 'forum_id'
          && isset($argument->argument) && is_numeric($argument->argument)) {
          $forum_id = (int) $argument->argument;
          break;
        }
      }
    }

    if (!$forum_id && !empty($this->view->args[0]) && is_numeric($this->view->args[0])) {
      $forum_id = (int) $this->view->args[0];
    }

    return $forum_id ? entity_load('entity_forum_forum', $forum_id) : FALSE;
  }

}
