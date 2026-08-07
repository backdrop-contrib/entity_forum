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
   * Description shown on forum listings, run through $description_format.
   * @var string
   */
  public $description = '';

  /**
   * Text format machine name for the description.
   * @var string
   */
  public $description_format;

  /**
   * The {file_managed}.fid of the forum's icon (0 = none).
   *
   * An uploaded image shown beside the forum name in the index, sub-forum
   * lists and at the top of the forum page. Managed-file bookkeeping (a
   * {file_usage} record keyed 'forum_icon'/forum_id, released on replace or
   * forum delete) is handled in entity_forum_forum_icon_apply().
   *
   * @var int
   */
  public $icon_fid = 0;

  /**
   * Image style for the icon on this forum's OWN page header.
   *
   * Empty means the shipped small entity_forum_forum_icon style — so a plain
   * forum stays small while a featured forum can carry a large left-hand icon.
   * Forum listings always render the small style regardless. Resolved through
   * entity_forum_format_forum_icon(), which falls back to the shipped style if
   * the named one has since been deleted.
   *
   * @var string
   */
  public $icon_style = '';

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
   * (e.g. an author marking their own support question resolved).
   * @var int
   */
  public $author_close = 0;

  /**
   * Whether the forum appears in the forums index and sub-forum lists
   * (1 = listed). Unlisted forums stay usable at their own URL, and take
   * their whole sub-tree out of the index with them; this is independent
   * of $status (published).
   * @var int
   */
  public $listed = 1;

  /**
   * The view display rendering this forum's topic list, as "name:display_id".
   *
   * Empty means the shipped default, so a site that never chooses one simply
   * follows the module. Set it to a copy of the topics view and a photo-gallery
   * forum renders as a thumbnail grid while the discussion forums stay a topic
   * table — the other half of "sitedevs copy and amend the shipped views".
   *
   * Read it through entity_forum_forum_topic_list_view(), which falls back to
   * the default if the named view or display has since been deleted: no forum
   * should ever end up with no topic list at all.
   *
   * @var string
   */
  public $topic_view = '';

  /**
   * The topic type every topic posted in this forum takes.
   *
   * Set on the forum, so posting never asks which kind of topic this is — a
   * support forum carries topics with a product-version field, a discussion
   * forum stays plain. A forum wanting a different kind of topic gets a
   * sub-forum.
   *
   * EMPTY (ENTITY_FORUM_FORUM_TOPIC_TYPE_NONE) means the forum takes no
   * topics at all: a container grouping sub-forums. Distinct from naming a
   * type that has since been deleted, which falls back to the default type —
   * same column, opposite outcomes. Read it through
   * entity_forum_forum_topic_type(), which resolves both.
   *
   * @var string
   */
  public $topic_type;

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
    // isset(), NOT empty(): '' is a meaningful value here — a container
    // forum that takes no topics — and empty('') is TRUE, so an empty()
    // check would silently retype every container back to the default on
    // load, because this constructor also runs when PDO builds a loaded row.
    // Only a genuinely new forum (no property at all) gets the default.
    if (!isset($this->topic_type)) {
      $this->topic_type = ENTITY_FORUM_TOPIC_TYPE_DEFAULT;
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
   *
   * Defers to entity_forum_forum_path(), which is the single definition of
   * a forum's address - the bulk callers reach it by ID without loading an
   * entity, and this keeps them and entity_uri() answering the same thing.
   */
  public function uri() {
    return array('path' => entity_forum_forum_path($this->forum_id));
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
   * The topic type (bundle): an entity_forum topic type machine name.
   *
   * Must never be empty — entity_extract_ids() throws EntityMalformedException
   * on a missing bundle, so the constructor falls back to
   * ENTITY_FORUM_TOPIC_TYPE_DEFAULT rather than let a topic exist without one.
   * Deliberately defaulted in the constructor and not here: a property default
   * is evaluated when the class is declared, which would depend on
   * entity_forum.module having been loaded first.
   *
   * @var string
   */
  public $type;

  /**
   * The topic title.
   * @var string
   */
  public $title = '';

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
    if (empty($this->type)) {
      $this->type = ENTITY_FORUM_TOPIC_TYPE_DEFAULT;
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
   * Implements EntityInterface::bundle().
   *
   * Topics are bundleable, so this overrides the base class — which returns
   * the entity type and would silently report every topic as the same bundle.
   */
  public function bundle() {
    return $this->type;
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
   * The reply type (bundle): an entity_forum topic type machine name.
   *
   * Derived from the reply's topic rather than chosen — a reply in an
   * Events topic is an Events reply — and denormalized onto the row
   * exactly like $forum_id. hook_entity_presave() is authoritative: it sets
   * this from the topic on every save, so nothing else has to remember to.
   * Defaulted in the constructor for the same reason as EntityForumTopic::$type.
   *
   * @var string
   */
  public $type;

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
    if (empty($this->type)) {
      $this->type = ENTITY_FORUM_TOPIC_TYPE_DEFAULT;
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
   * Implements EntityInterface::bundle().
   *
   * Replies are bundleable, so this overrides the base class — which returns
   * the entity type and would report every reply as the same bundle.
   */
  public function bundle() {
    return $this->type;
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
