<?php

/**
 * @file
 * Entity classes for the Entity Forum module.
 *
 * All three classes extend the global Entity base class provided by the core
 * entity module and are loaded by EntityPlusController (entity_plus module),
 * which instantiates them with ($values, $entity_type) via PDO::FETCH_CLASS.
 * Properties are intentionally untyped: PDO assigns raw string values from
 * the database before the constructor runs.
 */

/**
 * A forum container. Topics belong to a forum; forums may be nested.
 */
class EntityForumForum extends Entity {

  /**
   * Primary key.
   * @var int
   */
  public $forum_id;

  /**
   * The forum title.
   * @var string
   */
  public $title = '';

  /**
   * Plain-text description shown on forum listings.
   * @var string
   */
  public $description = '';

  /**
   * Published flag (1 = published, 0 = unpublished).
   * @var int
   */
  public $status = 1;

  /**
   * Parent forum ID (0 for top-level forums).
   * @var int
   */
  public $parent_id = 0;

  /**
   * Visibility: 'public' or 'private'.
   * @var string
   */
  public $visibility = 'public';

  /**
   * Sort order in forum listings (lower first).
   * @var int
   */
  public $weight = 0;

  /**
   * Created timestamp.
   * @var int
   */
  public $created;

  /**
   * Last-changed timestamp.
   * @var int
   */
  public $changed;

  /**
   * The {users}.uid of the forum author.
   * Negative values are a deleted account's "last uid".
   * @var int
   */
  public $author_uid;

  /**
   * Snapshot of the author display name at save time.
   * @var string
   */
  public $author_name = '';

  /**
   * Whether topic authors may close/reopen their own topics here
   * (classified ads: mark as sold).
   * @var int
   */
  public $author_close = 0;

  /**
   * Constructs a forum, filling runtime defaults for new entities.
   *
   * @param array $values
   *   Initial property values.
   * @param string $entity_type
   *   Ignored; the type is fixed. Present because EntityPlusController
   *   passes it as second constructor argument.
   */
  public function __construct(array $values = array(), $entity_type = NULL) {
    parent::__construct($values);
    if (!isset($this->created)) {
      $this->created = REQUEST_TIME;
    }
    if (!isset($this->changed)) {
      $this->changed = REQUEST_TIME;
    }
    if (!isset($this->author_uid)) {
      $this->author_uid = $GLOBALS['user']->uid;
    }
  }

  /**
   * Implements EntityInterface::id().
   */
  public function id() {
    return isset($this->forum_id) ? (int) $this->forum_id : NULL;
  }

  /**
   * Implements EntityInterface::entityType().
   */
  public function entityType() {
    return 'entity_forum_forum';
  }

  /**
   * Implements EntityInterface::label().
   */
  public function label() {
    return $this->title;
  }

  /**
   * Implements EntityInterface::uri().
   */
  public function uri() {
    return array('path' => 'forum/' . $this->forum_id);
  }

}

/**
 * A discussion topic: the opening post of a thread within a forum.
 */
class EntityForumTopic extends Entity {

  /**
   * Primary key.
   * @var int
   */
  public $topic_id;

  /**
   * The topic title.
   * @var string
   */
  public $title = '';

  /**
   * Body of the opening post.
   * @var string
   */
  public $content = '';

  /**
   * Text format machine name for the content.
   * @var string
   */
  public $format;

  /**
   * Published flag (1 = published, 0 = unpublished).
   * @var int
   */
  public $status = 1;

  /**
   * The forum this topic belongs to.
   * @var int
   */
  public $forum_id;

  /**
   * The {users}.uid of the topic author.
   * Negative values are a deleted account's "last uid".
   * @var int
   */
  public $author_uid;

  /**
   * Snapshot of the author display name at save time.
   * @var string
   */
  public $author_name = '';

  /**
   * Created timestamp.
   * @var int
   */
  public $created;

  /**
   * Last-changed timestamp.
   * @var int
   */
  public $changed;

  /**
   * Sticky flag: sticky topics sort first in the forum topic list.
   * @var int
   */
  public $sticky = 0;

  /**
   * Closed flag: closed topics accept no new replies.
   * @var int
   */
  public $closed = 0;

  /**
   * Topic list visibility: 'normal', 'admin_only' or 'unlisted'.
   * @var string
   */
  public $listing = 'normal';

  /**
   * Awaiting moderator review (held with listing = admin_only).
   * @var int
   */
  public $pending = 0;

  /**
   * Why the post was held for review (empty when not held).
   * @var string
   */
  public $pending_reason = '';

  /**
   * The node this topic is attached to as its discussion thread (0 = none).
   * @var int
   */
  public $node_id = 0;

  /**
   * Denormalized count of published replies (maintained automatically).
   * @var int
   */
  public $reply_count = 0;

  /**
   * Denormalized ID of the newest published reply.
   * @var int
   */
  public $last_reply_id = 0;

  /**
   * Denormalized timestamp of the latest activity (topic or reply).
   * @var int
   */
  public $last_active_time = 0;

  /**
   * Constructs a topic, filling runtime defaults for new entities.
   *
   * @param array $values
   *   Initial property values.
   * @param string $entity_type
   *   Ignored; see EntityForumForum::__construct().
   */
  public function __construct(array $values = array(), $entity_type = NULL) {
    parent::__construct($values);
    if (!isset($this->created)) {
      $this->created = REQUEST_TIME;
    }
    if (!isset($this->changed)) {
      $this->changed = REQUEST_TIME;
    }
    if (!isset($this->author_uid)) {
      $this->author_uid = $GLOBALS['user']->uid;
    }
    if (empty($this->last_active_time)) {
      $this->last_active_time = $this->created;
    }
  }

  /**
   * Implements EntityInterface::id().
   */
  public function id() {
    return isset($this->topic_id) ? (int) $this->topic_id : NULL;
  }

  /**
   * Implements EntityInterface::entityType().
   */
  public function entityType() {
    return 'entity_forum_topic';
  }

  /**
   * Implements EntityInterface::label().
   */
  public function label() {
    return $this->title;
  }

  /**
   * Implements EntityInterface::uri().
   */
  public function uri() {
    return array('path' => 'topic/' . $this->topic_id);
  }

}

/**
 * A reply to a topic (or, when reply_to_id is set, to another reply).
 */
class EntityForumReply extends Entity {

  /**
   * Primary key.
   * @var int
   */
  public $reply_id;

  /**
   * Body of the reply.
   * @var string
   */
  public $content = '';

  /**
   * Text format machine name for the content.
   * @var string
   */
  public $format;

  /**
   * Published flag (1 = published, 0 = unpublished).
   * @var int
   */
  public $status = 1;

  /**
   * The topic this reply belongs to.
   * @var int
   */
  public $topic_id;

  /**
   * The forum the topic belongs to (denormalized for querying).
   * @var int
   */
  public $forum_id;

  /**
   * The {users}.uid of the reply author.
   * Negative values are a deleted account's "last uid".
   * @var int
   */
  public $author_uid;

  /**
   * Snapshot of the author display name at save time.
   * @var string
   */
  public $author_name = '';

  /**
   * Created timestamp.
   * @var int
   */
  public $created;

  /**
   * Last-changed timestamp.
   * @var int
   */
  public $changed;

  /**
   * Parent reply ID when this is a threaded reply to another reply.
   * @var int
   */
  public $reply_to_id = 0;

  /**
   * Awaiting moderator review (held with status = 0).
   * @var int
   */
  public $pending = 0;

  /**
   * Why the post was held for review (empty when not held).
   * @var string
   */
  public $pending_reason = '';

  /**
   * Constructs a reply, filling runtime defaults for new entities.
   *
   * @param array $values
   *   Initial property values.
   * @param string $entity_type
   *   Ignored; see EntityForumForum::__construct().
   */
  public function __construct(array $values = array(), $entity_type = NULL) {
    parent::__construct($values);
    if (!isset($this->created)) {
      $this->created = REQUEST_TIME;
    }
    if (!isset($this->changed)) {
      $this->changed = REQUEST_TIME;
    }
    if (!isset($this->author_uid)) {
      $this->author_uid = $GLOBALS['user']->uid;
    }
  }

  /**
   * Implements EntityInterface::id().
   */
  public function id() {
    return isset($this->reply_id) ? (int) $this->reply_id : NULL;
  }

  /**
   * Implements EntityInterface::entityType().
   */
  public function entityType() {
    return 'entity_forum_reply';
  }

  /**
   * Implements EntityInterface::label().
   */
  public function label() {
    return t('Reply #@id', array('@id' => (string) $this->reply_id));
  }

  /**
   * Implements EntityInterface::uri().
   *
   * Replies have no page of their own; they link to their topic, anchored.
   */
  public function uri() {
    return array(
      'path' => 'topic/' . $this->topic_id,
      'options' => array('fragment' => 'reply-' . $this->reply_id),
    );
  }

}
