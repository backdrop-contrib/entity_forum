# Changelog — entity_forum

## (unreleased)

### Added (private-forum attachment access control, 2026-07-12)
- Attachments belonging to **private-forum** posts are now stored in
  `private://` and gated by `hook_file_download()`
  (`entity_forum_file_download`): a private attachment is served only to
  users who may view its forum ("view private entity forum"); anonymous
  and unauthorised users get a 403. Public-forum attachments stay
  `public://` and are served directly by the web server (unchanged).
  Requires the private file system path to be configured.
- `hook_update_1002` moves existing private-forum files `public:// →
  private://` (723 on this site; 170 public-forum files left in place) and
  flushes stale image derivatives. The importer's attachment step now
  writes each file to the correct scheme by forum visibility, so future
  imports are secured from the start.
- Stays correct as content moves: changing a **forum's** visibility
  re-schemes all its attachment files; moving a **topic** to another forum
  syncs its replies' denormalized `forum_id` and re-schemes the topic's
  and replies' files. Helpers: `entity_forum_rescheme_file()`,
  `entity_forum_rescheme_forum_files()`.
- Known limitation: **inline images** pasted into post bodies stay public
  (their URLs are baked into the content HTML and can't be gated without
  breaking them). Only the Attachments section is access-controlled.

### Changed (attachment display declutter, 2026-07-12)
- Image attachments now render as just the clickable thumbnail
  (redundant filename/size removed); non-image files show a filename link
  with the size removed. @todo (deferred pending a module choice): give
  non-image files (e.g. .mov) a poster/placeholder image sized like the
  thumbnails, and open attachments in a Colorbox-style lightbox.

### Added (per-field permissions via Field Permissions + reply attachments, 2026-07-12)
- Attachments on replies: mirrored the topic bundle's field_attachments
  instance onto entity_forum_reply, so repliers get the same upload widget
  (the reply form already ran field_attach_form). Previously only topic
  starters could attach.
- Dynamic per-field permissions are now handled by the contrib Field
  Permissions module (enabled): setting any field on a forum bundle to
  "Custom permissions" auto-generates create / edit / edit own / view /
  view own permissions for it, which appear on the permissions page as
  fields are added — no forum-specific code.
- hook_entity_load() now exposes the author as a standard 'uid' property
  (from author_uid; 0 for deleted authors) so Field Permissions' "own"
  variants correctly recognise a forum post's author — its ownership
  check keys off $entity->uid, which our entities lacked. Note: for
  forums the field-level "own" distinction is largely redundant with the
  existing entity-level own-post edit restriction; the non-own perms
  ("edit field_X", "create field_X") are the usual choice. Fields stay
  public (unrestricted) until an admin opts them into custom mode, so
  existing fields never fail closed.

### Changed (clearer permission descriptions, 2026-07-12)
- Added descriptions to the "Create Entity Forum topics/replies"
  permissions spelling out that they also cover editing/deleting the
  user's own posts, and clarified the "Administer …" permissions as the
  moderator (edit-any) capability. No behaviour change — edit access
  already worked correctly (own posts need only the create permission;
  editing others' needs the administer permission).

### Added (sub-forum polish, 2026-07-12)
- Sub-forums on a forum page now render as full rows (Subforum / Topics /
  Posts / Last activity, counts rolled up to include their own
  sub-forums) instead of a plain bullet list, matching the forums index.
- Breadcrumbs climb the full ancestor chain on both forum and topic pages
  (Home > Forums > ...parents... > forum), via new helpers
  entity_forum_forum_parents/_ancestors/_descendants() and the shared
  _entity_forum_set_breadcrumb(). Previously the topic page showed only
  the immediate forum and the forum page had no forum breadcrumb.
- Cycle fix: the parent-forum dropdown on the forum edit form now excludes
  the forum's whole descendant sub-tree (not just itself), so a forum can
  no longer be re-parented under one of its own children — which would
  have created an infinite loop in the index/rollup recursion.

### Added (forum count service + Views count fields, 2026-07-12)
- entity_forum_forum_counts(): a per-request + cache-bin cached map of
  every forum's topic/reply/post counts and last-activity, built from two
  grouped queries plus an in-PHP rollup so a parent forum's totals include
  all of its sub-forums (bbPress behaviour). Cache auto-invalidated on
  forum/topic/reply insert/update/delete (entity_forum_forum_counts_reset()
  in the entity CRUD hooks).
- The forums index now reads from the service instead of running a COUNT
  and a MAX query per forum row (removed that N+1); entity_forum_forum_topic_count()
  is now a thin wrapper over the map.
- New Views fields on the forum base table — Topic count, Post count,
  Reply count, Last activity (entity_forum_handler_field_forum_count) —
  all reading the cached map, so a forums-list View costs the same two
  queries whether it shows 4 forums or 40 (no per-row count queries, no
  need for AJAX/JS deferral). Enables building the main forums list in
  Views. Verified: counts match an independent rolled-up recount, the
  rollup picks up a nested sub-forum's posts, and the cache invalidates
  on save.

### Added (fieldable entity types + structure UI, 2026-07-12)
- The three entity types (already fieldable in hook_entity_info) are now
  fully wired to the Field API, so sites can add fields — attachments,
  ratings, whatever — through the normal Backdrop UI, no forum code
  needed (the same route forum_ng relies on):
  - New structure UI at admin/structure/entity-forum: landing page
    listing Forum / Topic / Reply, one page per type; the bundles declare
    admin paths there so Field UI attaches its "Manage fields" and
    "Manage display" tabs (works without field_ui; the tabs just
    disappear and the landing page says to enable it).
  - All six entity forms (frontend new-topic + reply; admin forum, topic,
    reply) run field_attach_form/validate/submit, so attached fields get
    real widgets and validation.
  - The topic page renders attached fields (view mode 'full') below each
    post's content in a .entity-forum-fields wrapper — batch-prepared for
    the page's replies via entity_load.
  - Field API fields on these entity types are automatically available to
    Views. Verified end-to-end with a disposable text field: Field UI
    tabs, widget on the reply form, value persisted through the
    EntityPlusController save/load cycle, display on the topic page, and
    Views data — then cleaned up.

### Added (default forum picture + settings form, 2026-07-12)
- Members without their own forum picture (including former members and
  unknown authors) now show a default: an admin-uploaded image when
  configured, otherwise the module's shipped SVG silhouette
  (images/avatar-default-symbolic.svg, rendered as a plain sized image —
  SVGs can't pass through image styles).
- New Settings tab at admin/content/entity-forum/settings
  (entity_forum_settings_form, 'administer entity forum'), backed by the
  new entity_forum.settings config (hook_config_info; shipped default in
  config/). Currently holds the default-picture upload with preview;
  intended home for future forum settings.

### Changed (attachments require login, 2026-07-12)
- Attachment lists on posts are now only shown to logged-in members,
  matching the old bbPress site: anonymous visitors see "Attachments:
  You must be logged in to view attached files." with a login link
  (destination returns them to the topic). Note: files themselves remain
  public:// until the planned private-attachments task, so direct URLs
  still work for anyone who has them.

### Added (forum pictures, 2026-07-12)
- Members can upload a "Forum picture" on the account Forum tab (PNG/JPG/
  GIF ≤ 2 MB, with current-picture preview). Module-owned by design — a
  managed file whose fid is stored in the account data array
  ('entity_forum_avatar_fid'), deliberately NOT core's user picture or a
  site-specific field, so importers can seed it (e.g. from WordPress
  avatars) via entity_forum_avatar_save(), which handles all the
  managed-file bookkeeping (permanent status, file_usage, deleting the
  replaced file). Cleared + file deleted on account deletion.
- Pictures render on topic-page posts (96×96, floated beside the post)
  and in the topic-list view beside "Started by" and "Last post by" names
  (28×28) via a new Views field handler (entity_forum_handler_field_avatar,
  "Author picture" on topics and replies, image style selectable).
  Two image styles shipped in config: entity_forum_avatar (96×96) and
  entity_forum_avatar_small (28×28). New css/entity_forum.css attached on
  the frontend pages. Deleted/unknown authors render no picture.
  Fixed same day: zero-reply topics showed two icons in "Last post" —
  Views tokens carry the REWRITTEN output of earlier fields, so the
  fallback "by [author_name]" must not add [author_avatar] (the
  Started-by rewrite already embeds it).

### Added (Views conversion of the topic list, 2026-07-12)
- The forum-page topic list is now a View (entity_forum_topics, shipped in
  config/) embedded by entity_forum_forum_page(), so site builders can
  edit the columns at admin/structure/views. Columns replicate the old
  bbPress list: Topic (link + 📎 attachment marker + Sticky/Closed
  markers), Started by (author_name snapshot), Voices, Replies, Last post
  ("time ago by <name>", falling back to the topic author when there are
  no replies). Sticky first, then latest activity; pager 25; click-sortable.
  Breadcrumbs, subforums, "Post new topic" and private-forum access checks
  stay in the page callback. New dependency: views (core).
- New denormalized topic columns voice_count (distinct participants,
  bbPress "Voices") and attachment_count ({file_usage} attachments on the
  topic and its published replies), maintained by
  entity_forum_topic_recalculate_stats() and backfilled by
  hook_update_1001 (batched).
- hook_entity_property_info_alter() now also types the reference columns
  (author_uid → user; topic forum_id / reply forum_id / forum parent_id →
  forum; last_reply_id / reply_to_id → reply; reply topic_id → topic), so
  Views gets real relationships (e.g. topic → last reply → author), plus
  human labels for the denormalized columns.

### Added (alter hooks for other modules, 2026-07-12)
- New alter hooks so other modules can change the frontend pages:
  hook_entity_forum_forums_page_alter(), hook_entity_forum_forum_page_alter()
  and hook_entity_forum_topic_page_alter(). Documented in the new
  entity_forum.api.php, along with the form IDs for altering the new-topic,
  reply and posting-name forms via hook_form_FORM_ID_alter().

### Fixed (Views dates, 2026-07-12)
- Timestamp columns (created, changed, topic last_active_time) showed as
  raw numbers in Views. hook_entity_property_info_alter() now declares
  them as 'date' properties, so entity_plus generates date field/sort/
  filter/argument Views handlers for all three entity types. Needs a
  cache flush to take effect on existing sites.

### Added (signatures on posts, 2026-07-12)
- Posts now show the author's signature at the bottom (below content and
  attachments), following core comment behaviour: only when the site-wide
  "Enable signatures" setting is on (admin/config/people/settings) and the
  author is a live account with a signature set. Rendered at display time
  via entity_forum_format_signature() — post content in the DB is
  untouched, and signature edits apply to all of the member's posts.
  Deleted/unknown authors (uid <= 0) never render one.

### Changed (account Forum tab, 2026-07-12)
- The "Forum posts" account tab is renamed "Forum" and the "Forum posting
  name" field moved onto it from the account edit form (top of the tab, as
  its own small form). The save logic lives in the reusable
  entity_forum_posting_name_save(); the user_profile_form alter/submit
  were removed.

### Changed (rename)
- Module renamed `bb_forum` → `entity_forum` before first release: "bb"
  suggested bbPress, and the new name fits the entity_plus/entity_ui family
  without conflicting with the forum/forum_ng modules. Entity types, tables,
  classes (EntityForumForum etc.), permissions ("administer entity forum"
  etc.), admin path (admin/content/entity-forum) and CSS classes all
  renamed. Frontend URLs unchanged. Applied on the dev site via uninstall →
  reinstall → reimport; the obsolete weight hook_update_1000 was folded into
  the base schema.

### Added (member deletion protection & attribution)
- Forum content can no longer be deleted by removing a member account.
  hook_user_predelete() (catch-all for every deletion path) keeps the
  posts: the author_name snapshot is filled from the account being
  deleted and author_uid is flipped to its negative "last uid" — making
  former-member posts queryable (author_uid < 0, e.g. in a View) and
  reinstatable via entity_forum_reassign_user_content(-$old, $new). The
  account-cancel confirmation forms show the member's forum post count
  and explain the keep policy. author_uid is now signed.
- author_name snapshot column on all three tables, auto-filled on every
  save (hook_entity_presave) using the display chain: forum posting name
  → account name. The chain is deliberately free of site-specific fields
  (the WP field_display_name dependency was removed — general sites
  won't have it; the WP importer seeds posting names instead). Bylines
  render live accounts with their display name linked to the profile;
  gone accounts as "Name (no longer a member)"; unknown authors (uid 0,
  e.g. authors already deleted from the old WordPress site) as
  "No longer a member" — no conflation with real anonymous users.
- "Forum posting name" field on the account edit form: members can post
  under a handle instead of their real/login name; changing it refreshes
  the snapshots on their existing posts.
- "Forum posts" account tab (user/UID/forum-posts): members (and user
  admins) see the account's topics and replies, paged, with links.
- Hardened own-post edit access: requires a logged-in account, so uid-0
  orphan posts are never editable by anonymous visitors.

### Added (attachments display)
- Topic and reply posts now render an Attachments section from
  {file_usage} entries (module 'entity_forum'): images as thumbnail-style
  derivatives linking to the original, other files as download links with
  size. Populated by entity_forum_importer; a future upload UI can reuse
  the same mechanism.

### Added (recommendations)
- Status report check "Entity Forum image library privacy": when any text
  format allows editor image uploads and the Image Library Image Access
  module is not enabled, recommends installing it (the core image library
  dialog otherwise shows every site image to anyone who can browse it).

### Added (Phase 2 support)
- `weight` column on {entity_forum_forum} for explicit forum ordering
  (hook_update_1000); forum listings now order by weight, then title; the
  admin forum form exposes it. Populated from bbPress menu_order on import.
- Content migrated on the dev site by the new entity_forum_importer module: 4 forums,
  1,569 topics, 5,790 replies from tr3test (users first via acuity_wpuser —
  1,015 users).

### Changed
- Complete rewrite of the module in native Backdrop 1.x style. The previous
  Gemini-generated code was written against fictional Drupal 8/9-style APIs
  (namespaced `Backdrop\Core\Entity` classes, `EntityForm`, `FormState`)
  that do not exist in Backdrop and could never load.
- Entity classes are now plain global classes extending core `Entity`,
  registered via `hook_autoload_info()` and managed by `EntityPlusController`
  (entity_plus module): `EntityForumForum`, `EntityForumTopic`, `EntityForumReply` in
  `includes/entity_forum.entity.inc`.
- Forms converted to procedural Form API builders in `entity_forum.admin.inc`.
- Dependencies corrected: `entity` (core) + `entity_plus`. The entity_ui
  bundleable controller was dropped (single-bundle entities, own admin UI).

### Added
- Frontend pages (`entity_forum.pages.inc`): `forums` index, `forum/%` topic
  list (sticky topics first, paged), `topic/%` with replies, inline reply
  form (threading via `?reply_to=N`), and a frontend new-topic form.
- Admin UI as a "Forums" tab on admin/content with Forums / Topics / Replies
  overviews and add/edit/delete forms.
- Schema: `sticky` and `closed` columns on `entity_forum_topic`, `format`
  columns on topic and reply for text-format handling, composite listing
  indexes, and the `entity_forum_migrate_map` table (legacy WP ID → entity ID)
  for the upcoming importer.
- Automatic maintenance of denormalized topic statistics (`reply_count`,
  `last_reply_id`, `last_active_time`) via `hook_entity_insert/update/delete`;
  topic deletion cascades to its replies; forum deletion is blocked while
  topics or child forums remain.

### Removed
- `src/Backdrop/entity_forum/` duplicate class tree, `Controller/` and `Form/`
  directories, and the root-level namespaced entity classes.

### Added (conflict protection)
- `hook_requirements()`: installation is blocked with an error while the
  `forum` or `forum_ng` module is enabled; the status report shows two
  runtime checks — conflicting modules, and whether the `forums` path is
  actually served by Entity Forum (catches leftover views overriding it).
- `hook_modules_enabled()`: immediate admin warning + watchdog entry if a
  conflicting forum module is enabled while Entity Forum is active.

### Site cleanup (dev site, 2026-07-11)
- forum_ng was uninstalled by the user; its leftovers were removed with
  approval: test node 12, the `forum_ng` content type (removing
  /node/add/forum-ng), the `forum_ng` vocabulary and 4 terms, the
  `forum_ng_taxonomy` / `field_private_forum` fields, both forum_ng views,
  and `addanother.forum_ng` config. The `forums` path is now served by
  Entity Forum (verified over https).
- settings.php: converted to `$databases` array format and added the
  `$databases['wordpress']` connection pointing at `tr3test` for the
  Migration Centre (verified via acuity_wpmc_get_database()).

### Notes
- The module was cleanly uninstalled and reinstalled on the dev site to apply
  the new schema (all tables were empty). Verified: entity CRUD round-trip,
  reply-counter recalculation, cascade delete, anonymous access to public
  pages, 403 on admin pages, 404 on missing entities.
- Known issue: the `forums` path is shadowed by the enabled forum_ng module's
  view (`views.view.forum_ng`). Either disable forum_ng or move/disable that
  view before launch. `forum/%` and `topic/%` are unaffected.
