# entity_forum — Module Notes

**Renamed 2026-07-11:** `bb_forum` → `entity_forum` (and `bb_importer` →
`entity_forum_importer`) before public release — "bb" read as bbPress, and
the new name fits the entity_plus / entity_ui family the module is built
on without conflicting with `forum` / `forum_ng`. The rename was done as
uninstall → rename code → reinstall → reimport (the importer makes content
disposable). Entity types, tables, permissions, admin paths and CSS classes
all carry the new prefix; frontend URLs (forums, forum/N, topic/N) were
never module-prefixed and are unchanged.

## Purpose

To create a new website Forum with the ability to import an existing WordPress BBForum's content.
Use backdrop /modules/forum_ng for inspiration when needed

There is a backup DB for the existing WordPress site we can use for testing on old data...  Look at tr3test in localhost SQL

---

## CURRENT STATE (2026-07-11)

**Phase 1 complete: the module has been fully rewritten in native Backdrop
1.x style and verified working.** The previous Gemini-generated Drupal 8/9
style code (fictional namespaces, class-based forms) was deleted.

Architecture:

- Three single-bundle entity types on `EntityPlusController` (entity_plus):
  `entity_forum_forum`, `entity_forum_topic`, `entity_forum_reply`. Entity classes are
  plain global classes extending core `Entity` in
  `includes/entity_forum.entity.inc`, registered via `hook_autoload_info()`.
- Dependencies: `entity` (core), `entity_plus`. entity_ui is NOT used.
- Admin UI: "Forums" tab on admin/content (`entity_forum.admin.inc`) with
  Forums / Topics / Replies overviews and procedural add/edit/delete forms.
- Fieldable (fully wired 2026-07-12): structure UI at
  admin/structure/entity-forum (bundle admin paths → Field UI tabs; sites
  add attachment/rating/etc. fields with zero forum code). All six entity
  forms run field_attach_form/validate/submit; the topic page renders
  attached fields ('full' view mode) below post content
  (_entity_forum_render_fields(), batch entity_load for replies); fields
  are Views-able automatically. Historical bbPress attachments stay in
  {file_usage} (login-gated display) — a future importer option could
  populate a real file field instead. A `field_attachments` file field
  (≤5 files, jpg/png/pdf/doc, 6 MB) is on BOTH topic and reply bundles
  (reply instance added 2026-07-12).
- Per-field permissions: use the contrib **Field Permissions** module
  (enabled 2026-07-12). Set a field to "Custom permissions" and it
  auto-generates create/edit/edit-own/view/view-own perms per field —
  the "dynamic permissions as fields are added" requirement, no custom
  code. Bridge: hook_entity_load() sets $entity->uid = author_uid
  (0 for deleted authors) because Field Permissions' ownership check is
  hardcoded to $entity->uid and our entities use author_uid. Field-level
  "own" is mostly redundant with the entity-level own-post edit gate in
  entity_forum_topic_access/_reply_access; fields stay public until opted
  into custom mode (no fail-closed).
- Frontend (`entity_forum.pages.inc`): `forums` index, `forum/%` (subforums
  + the embedded entity_forum_topics view — see PLANNED/NEXT "DONE
  2026-07-12"), `topic/%` with replies (paged 50) + inline reply form;
  threading via `?reply_to=N`. Private forums require the
  'view private entity forum' permission; topics inherit forum visibility.
  Page render arrays are alterable via custom hooks (entity_forum.api.php).
  Sub-forums (polished 2026-07-12): forum pages show sub-forums as full
  count-bearing rows (from the counts map); breadcrumbs climb the whole
  ancestor chain on forum + topic pages via
  entity_forum_forum_parents/_ancestors/_descendants() +
  _entity_forum_set_breadcrumb(); the forum-edit parent dropdown excludes
  the forum's descendants (cycle protection).
  Posts show the author's core user signature at the bottom (2026-07-12):
  render-time via entity_forum_format_signature(), gated on the site-wide
  user_signatures setting (enabled on this site) + a live account with a
  signature; uid <= 0 authors never render one.
- Denormalized topic stats (`reply_count`, `last_reply_id`,
  `last_active_time`) are recalculated automatically in
  `hook_entity_insert/update/delete` (entity_forum_topic_recalculate_stats()),
  so the importer gets them for free.
- Forum-level aggregate counts are NOT denormalized: entity_forum_forum_counts()
  (2026-07-12) builds the whole `[forum_id => topic/reply/post_count,
  last_active_time]` map in two grouped queries + an in-PHP rollup of
  descendants into parents (bbPress-style), cached per-request and in the
  cache bin, invalidated on any forum/topic/reply CRUD. Chosen over
  denormalized columns because the forums index is a handful of rows —
  no per-row COUNT (the index page used to be N+1; now it reads the map),
  no schema/recalc maintenance. Exposed to Views as Topic/Post/Reply
  count + Last activity fields (entity_forum_handler_field_forum_count),
  which read the map so a forums-list View never does per-row counts (the
  reason we did NOT need AJAX/JS-deferred counts). Contrast: the topics
  list DID get denormalized columns because it has thousands of rows. Topic delete cascades to replies;
  forum delete is blocked while topics/children remain.
- Schema: topic has `sticky`, `closed`, `format`; reply has `format`;
  `entity_forum_migrate_map` (entity_type + wp_id → backdrop_id) is part of the
  schema for idempotent imports.

Verified on the dev site: clean reinstall, entity CRUD round-trip, counter
recalc, cascade delete, anonymous 200 on public pages / 403 on admin / 404
on missing entities. Content is fully imported (see Phase 2 below).

**forum_ng conflict — RESOLVED (2026-07-11):** forum_ng has been uninstalled
and all its leftovers deleted with user approval (test node, `forum_ng`
content type, vocabulary + terms, fields, views, addanother config). The
`forums` path is now served by entity_forum (verified over https). Guards added:
`hook_requirements()` blocks installing entity_forum while `forum`/`forum_ng` is
enabled and adds two status-report checks (conflicting modules; `forums`
path ownership); `hook_modules_enabled()` warns if a conflicting module is
enabled later. Note: `$databases['wordpress']` → tr3test is now configured
in settings.php and verified via acuity_wpmc.

**Dev-site checks:** use https (self-signed):
`curl -sk --resolve forum.test:443:192.168.0.10 https://forum.test/<path>`
— Apache binds to the LAN IP only, not localhost.

**Source data (verified 2026-07-11):** bbPress content in local MariaDB DBs
`trident` and test copy `tr3test` (prefix `wp_`): 4 forums (2 public /
2 private), ~1,600 topics, ~5,870 replies, 702 authors (all registered),
1012 wp_users. Threading via `_bbp_reply_to` postmeta (926), attachments via
`_bbp_attachment` postmeta (792 — handling deferred to Phase 3). Topics →
forums and replies → topics both via `post_parent`. `settings.php` defines
`$databases['wordpress']` → tr3test (connection verified via acuity_wpmc).
Also local: `cb1100r` DB, a different bbPress site (prefix `wpoa_`) by the
same developer, to migrate later — the importer must be table-prefix-aware.

**Site URLs:** original live site https://www.tr3oc.com (old forum URLs are
slug-based, e.g. /forum/which-triple-do-you-ride/ — relevant to the
redirects task); staging/live-test site https://trident.albanytest.co.uk —
this will become the final site moved onto the client's domain.

**Related modules:** `acuity_wpmc` (WP DB connection + migration dashboard)
and `acuity_wpuser` (user importer; stores legacy WP ID in `field_wp_guid`,
which provides the author-uid mapping for content attribution).

---

**Phase 2 complete (2026-07-11):** the importer was built as the sibling
`entity_forum_importer` module (multi-source design) and
the tr3test migration has been run on this dev site: 1,015 users (via
acuity_wpuser), 4 forums, 1,569 topics, 5,790 replies, 920 threaded.
Idempotent re-run verified; counters, ordering, privacy, statuses and
timestamps all verified. A `weight` column was added to {entity_forum_forum}
(hook_update_1000) to carry bbPress menu_order. See entity_forum_importer/CLAUDE.md.
Note: 819 topics have authors deleted from the old wp_users; they import
with uid 0 + empty author_name and render as "No longer a member" (see
Member deletion protection & attribution below).

## PLANNED / NEXT

### DONE 2026-07-12 — Views conversion of the topic list

The forum/N topic list is now the **entity_forum_topics** view, shipped in
config/views.view.entity_forum_topics.json and embedded by
entity_forum_forum_page() via views_embed_view() with the forum ID
argument. Breadcrumbs, subforums, "Post new topic" and private-forum
access checks stay in the page callback. Columns: Topic (link + 📎 when
attachment_count > 0 + (Sticky)/(Closed) markers merged into the column),
Started by (author_name snapshot — robust for deleted/unknown authors),
Voices, Replies, Last post ("time ago by <name>" via the last_reply_id
relationship, "No results text" falls back to the topic author token for
zero-reply topics). Sticky DESC then last_active_time DESC, pager 25,
click-sortable. New core dependency: views.

Supporting work, all in place:
- hook_entity_property_info_alter() (entity_forum.module) types
  created/changed/last_active_time as 'date' and the reference columns as
  entity types (author_uid → user; forum_id/parent_id → forum;
  last_reply_id/reply_to_id → reply; reply topic_id → topic), giving
  Views date handlers and real relationships.
- voice_count + attachment_count denormalized topic columns, maintained
  by entity_forum_topic_recalculate_stats(), backfilled by
  hook_update_1001 (batched; 1,569 topics on this site).
- Gotchas learned: Views data caches in cache_views — flush after
  property-info changes. Numeric Views fields render 1,000-separators by
  default — the excluded topic_id field used as a link token needs
  'separator' => ''. Field "No results text" DOES support tokens from
  earlier fields, but only fires with hide_alter_empty = TRUE.
- The view was built programmatically then exported (scratchpad
  build_view.php pattern) — safer than hand-writing views JSON.
- Alter hooks added for other modules (see entity_forum.api.php):
  hook_entity_forum_forums_page_alter / _forum_page_alter /
  _topic_page_alter; forms are alterable via hook_form_FORM_ID_alter
  (entity_forum_topic_frontend_form, entity_forum_reply_frontend_form,
  entity_forum_posting_name_form).
- The topic view page (posts + reply form) stays custom — threading and
  forms don't belong in Views. Forums index could follow later.

### DONE 2026-07-12 — Private attachment files

Private-forum attachments now live in `private://`, gated by
`hook_file_download()` (`entity_forum_file_download`): served only to
users who may view the forum ("view private entity forum"); anon → 403.
Public-forum attachments stay `public://` (web-server-served). Private FS
path is configured (`../private` → C:\laragon\www\private, outside the
webroot). `hook_update_1002` moved existing files (723 private → private://,
170 public left in place); the importer's attachment step now picks the
scheme by forum visibility. Kept correct on moves: forum-visibility change
re-schemes the forum's files; topic-forum change syncs replies' forum_id +
re-schemes (entity_forum_entity_update). Verified: anon 403 on the private
`/system/files/...` URL, member/admin granted, public 200, and the moved
file's OLD public URL now 404s (exposure closed). Image thumbnails render
via the gated `system/files/styles/.../private/...` derivative path.
**Known limitation kept:** inline images in post bodies stay public (URLs
baked into content) — only the Attachments section is gated.

### Backlog — forum / forum_ng converter source in entity_forum_importer

Second source inside `entity_forum_importer` (same-database — no external
DB, so the acuity_wpmc dependency should become per-source at that point).
Migrates node+taxonomy-based Backdrop forums into entity_forum. One codebase
covers both the contrib `forum` module (D7 core port) and `forum_ng`, since
they share the same architecture. Useful for other sites and as an upgrade
path if entity_forum is released on GitHub.
- Taxonomy terms → entity_forum_forum (term hierarchy → parent_id; forum_ng's
  `field_private_forum` → visibility).
- Topic nodes → entity_forum_topic (sticky maps directly; node status → status).
- Comments → entity_forum_reply (comment pid threading → reply_to_id).
- Configurable vocabulary + content type names; idempotent via
  `entity_forum_migrate_map` with distinct entity_type keys.
- Reuses the existing batch/map plumbing in entity_forum_importer.

Site-owner workflow (validated 2026-07-11 against Backdrop behaviour):
1. Disable forum_ng (content survives: nodes/terms/comments are core
   entities; the attached fields are core field types so they stay
   readable). entity_forum's hook_requirements() enforces this order — it
   blocks installation while forum_ng is enabled.
2. Enable entity_forum + the converter module.
3. Run the import (reads node/taxonomy/comment tables directly; forum_ng
   does not need to be active). Original content stays untouched — safe to
   re-run via the migrate map.
4. Test entity_forum.
5. Run the converter's "clean up source data" step (REQUIRED FEATURE:
   confirm form, offered only when all source items exist in the migrate
   map; deletes source nodes/comments, terms + vocabulary, the content
   type, and leftover views). Without this, uninstalling forum_ng orphans
   its content type (leaving /node/add/forum-ng), nodes, vocabulary and
   views — exactly the leftovers we cleaned manually on this dev site.
6. Uninstall forum_ng, then uninstall the converter.

### Phase 3

- **Attachments — DONE (2026-07-11).** Files migrated by
  entity_forum_importer (755 attachments + 138 inline files; 80 posts'
  URLs rewritten to local /files/ paths). entity_forum renders an
  Attachments section on posts from {file_usage} (module 'entity_forum'):
  image thumbnails linking to originals, download links for other types.
  Since 2026-07-12 the list is login-gated like the old bbPress site —
  anonymous visitors get a "must be logged in" prompt instead. Private-
  forum attachment FILES are now also secured: stored private:// and
  access-gated by hook_file_download (see "DONE 2026-07-12 — Private
  attachment files"). Public-forum files remain public://. Display was
  also decluttered 2026-07-12 (images = thumbnail only; files = filename
  link; no size/filename metadata).
- ~~forums path collision with forum_ng~~ — resolved earlier (forum_ng
  uninstalled and cleaned).

### Member deletion protection & attribution (DONE 2026-07-11)

Forum content survives account deletion with full attribution:
- `author_name` snapshot column on all three tables, auto-filled on save
  (hook_entity_presave). Display chain: **forum posting name** (user data
  'entity_forum_posting_name', set on the account "Forum" tab — a public
  handle) → account login name. **Deliberately generic** — entity_forum
  uses NO site-specific/WP fields (audited 2026-07-11: field_display_name
  was removed from the chain; general sites won't have it). Importers
  that know a better name seed the posting name instead: the WP importer
  seeds it from field_display_name, once, never overwriting a member's
  own choice (266 members seeded on this site).
- On account deletion (`hook_user_predelete()`, catch-all): snapshot is
  ensured, then author_uid is **flipped negative** ("last uid";
  author_uid is now signed). Former-member posts are queryable with
  author_uid < 0 (Views-ready) and reinstatable via
  `entity_forum_reassign_user_content(-$old_uid, $new_uid)`.
- Bylines: live account → display name linked to profile; deleted →
  "Name (no longer a member)"; unknown (uid 0, WP-orphaned imports) →
  "No longer a member". No conflation with real anonymous users.
- "Forum" account tab (user/UID/forum-posts, owner + user admins; renamed
  from "Forum posts" 2026-07-12): the forum profile form sits at the top
  of the tab (moved off user/UID/edit) — "Forum posting name" plus the
  "Forum picture" upload (managed_file, preview, ≤ 2 MB) — followed by
  the member's topics and replies. Names save through
  entity_forum_posting_name_save() (refreshes author_name snapshots);
  pictures through entity_forum_avatar_save().
- Forum pictures (2026-07-12): module-owned managed file, fid in the
  account data array ('entity_forum_avatar_fid') — deliberately not
  core's user picture. Rendered on posts (entity_forum_avatar style,
  96×96, css float) and in the topics view beside Started-by/Last-post
  names (entity_forum_avatar_small, 28×28) via the custom Views handler
  entity_forum_handler_field_avatar (views/ dir, autoloaded,
  hook_views_data_alter adds "Author picture" to topics + replies).
  Cleanup on account delete via predelete → entity_forum_avatar_save(0).
  Importer seeding planned (see entity_forum_importer/CLAUDE.md).
  Default picture fallback chain: member's file → admin-uploaded default
  (entity_forum.settings avatar_default_fid, via image style) → shipped
  SVG silhouette (images/avatar-default-symbolic.svg, plain <img> sized
  from the style's effect data — SVGs can't pass through image styles).
- Settings form at admin/content/entity-forum/settings
  (entity_forum.admin.inc, entity_forum.settings config,
  hook_config_info) — grow future forum settings here. CLI gotcha
  learned: never use $config as a top-level variable in bootstrap
  scripts; it clobbers the global settings.php overrides array and
  breaks Config's constructor.
- Importer fallback uid is now 0 (importer also snapshots author_name
  from the migrated user's display name). The 819 WP-orphaned topics
  render as "No longer a member", matching the old site's "Anonymous".
- Own-post edit access requires uid > 0 (anonymous can never edit
  uid-0 orphans).
- Gotcha learned: after hook_update_N adds columns, rebuild the schema
  cache (backdrop_get_complete_schema(TRUE)) before entity saves —
  backdrop_write_record silently drops fields missing from the cached
  schema.

### Planned — membership module (sidenote from Steve, 2026-07-11)

A separate "membership" module will administer club memberships and
renewals. Integration point with entity_forum: it can form_alter the same
cancel forms (or wrap its own member-removal flow) to offer richer choices
for a leaving member's forum posts (e.g. attribute to a named "Archived
member" account instead of Anonymous) before entity_forum's predelete
safety net runs.

### Later

- ~~Redirects from old bbPress slug URLs~~ — **assessed & DROPPED 2026-07-12.**
  Old URLs were `/forum/{slug}/` and `/topic/{slug}/` (bbPress
  `_bbp_include_root=0`); all slugs present and cleanly mappable via the
  migrate map, and the core `redirect` module is enabled — so it was
  cheap to build. But the whole point was preserving indexed/inbound
  links, and a `site:www.tr3oc.com inurl:/topic/` search returns **zero**
  indexed topic URLs (only ~20 pages of the whole site are indexed, none
  of them forum topics). Private forums (2 of 4; ~1,353 topics) aren't
  indexable anyway. No SEO value to preserve → not worth the work. The new
  site replaces the old on the same domain, so if this is ever revisited,
  path redirects would work.
- Search indexing; Views polish; templates and CSS for nicer forum
  styling (current output is clean but table-based).
- Attachment upload UI for new posts (reuse the file_usage mechanism the
  display already supports).
- forum/forum_ng converter source in entity_forum_importer (deferred to
  later in the dev cycle).
