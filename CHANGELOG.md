# Changelog - Entity Forum

Notable changes per release. The unreleased section accumulates change
summaries between pushes and doubles as the text for commit / release
descriptions.

## 1.0.0-beta3 (unreleased)

- **The forum icon is now a Views field.** "Forum icon" can be placed in any
  view on the forum base table, and reached from a topic or reply view through
  the forum relationship — so a classified ads listing, a forum block or a
  custom index can show the icon wherever it suits. It renders at each forum's
  own icon style by default, or at one style chosen for the view; forums with
  no uploaded icon show the shipped default, and SVG icons are sized rather
  than processed, exactly as elsewhere. Previously the icon was only reachable
  inside the forums-index identity cell.
- **Forum pickers now show the hierarchy.** The "Parent forum" select on the
  forum edit form, and the "Discussion forum" select on the settings page,
  list forums in tree order — each forum followed by its own sub-forums,
  indented one dash per level — instead of one flat alphabetical run.
  Siblings sort by weight then title, so the picker reads in the same order
  as the forums index.
- **Fixed: unticking "Appear in forum list" on a forum promoted its sub-forums
  to the top of the forums index** instead of hiding them. The index is built
  with the Forum hierarchy style, which treats a forum whose parent is missing
  from the list as a top-level forum — so hiding a container pushed its
  children up to root level, the opposite of what the setting says. An
  unlisted forum now takes its whole branch out of the index with it, however
  deep. The branch is unaffected everywhere else: each forum still lists its
  own sub-forums on its own page, so an unlisted section browses normally once
  you are in it — which is what lets you present it under its own menu link or
  a view of your own. The rule applies to everyone including administrators
  (the index is a members' menu, not an admin screen); the forum admin
  overview still shows every forum. Any forum view you build gets the rule if
  it filters on Listed, and dropping that filter opts out.
- **A forum's page address is now defined in one place** —
  `entity_forum_forum_path()`, which `EntityForumForum::uri()` and every
  breadcrumb, forum link and post-submit redirect resolve through, instead of
  six copies of the same composed path. No behaviour change; it means a
  forum's address can later be made configurable without hunting through
  renderers, a Views handler and two confirm forms.
- **The "Post new topic" link on a forum page now carries classes** —
  `entity-forum-new-topic-link button`, the same pair the Views area handler
  has always emitted, so one theme rule styles the link into a button wherever
  it appears. Previously the forum page's own link had no classes at all and
  could not be targeted.
- **Stopped shipping the classified image/file fields with File (Field) Paths
  settings baked in** — the `filefield_paths*` keys are removed from both
  shipped field instances. Entity Forum already places attachments in tidy,
  unique, privacy-correct paths; a module that rewrites a file's path or name on
  save fights the private-forum re-scheme and can leak private files into the
  public path. Existing installs are unchanged (the keys only ever took effect
  where filefield_paths was also installed).
- **README:** added a prominent Limitations warning against modules that rename
  or relocate managed files (File (Field) Paths the common example) — they can
  push a private forum's files into the public path (a privacy leak) or collide
  on the unique names the module and its importer rely on.

## 1.0.0-beta2 (2026-07-22)

Git tag: `1.x-1.0.0-beta2`.

- Forum icons: each forum shows an icon beside its name in the forums index,
  sub-forum lists and at the top of its page. Upload one per forum
  (PNG/JPG/GIF/SVG) on the edit form, or use the shipped default. Each forum
  also picks the image style (size) its icon renders at, everywhere it appears
  — a top-level forum can go large, sub-forums stay small (suggested, not
  forced); a "Forum icon (large)" 80×80 style ships ready-made. The icon sits
  in its own column so descriptions never wrap under it. Adds `icon_fid` and
  `icon_style` columns (`entity_forum_update_1000/1001()`) with full file-usage
  bookkeeping.
- Refined the shipped Forums and Topics views (column labels, Sticky/Closed
  markers, an attachment indicator and CSS-class wrappers for styling). Edited
  in the Views UI and folded back in as the module defaults.

## 1.0.0-beta1 (2026-07-19)

First public beta: a complete, fieldable forum for Backdrop CMS built on
custom entities (forums, topics, replies), replacing an imported bbPress
forum and usable as a core-comments replacement.

- Nested forums, public or private; containers ("category" forums) group
  sub-forums without taking topics.
- Topic types work like content types: each carries its own fields (and its
  replies their own, separate collection). The forum sets the type, so
  posting never asks. Post bodies are a real field, configurable per type.
- Access enforced in the data layer, not in renderers: a Views query alter
  covers all three base tables (private forums, listing states, published
  status), and a file gate + automatic re-scheming keeps private forums'
  attachments in the private file system.
- Moderation: per-member posting status (review / suspended), a hold queue
  with approve / keep-hidden actions, member content reports with
  moderator-only highlighting, moderation panels above forum lists.
- Comments replacement: node-attached discussions per content type (every
  item, or per-node opt-in via a checkbox on the node form).
- Views-native: the forums index, all admin overviews and per-forum topic
  lists are shipped views, built to be copied and amended; includes a
  "Forum hierarchy" tree style and handlers for rolled-up counts, author
  pictures, human-readable labels and per-viewer header areas.
- Member deletion protection: forum content survives account deletion with
  full attribution (name snapshots, negative "last uid"), reinstatable to a
  new account.
- Removable starter content (a plain forum plus a fielded classified-ads
  demo), cascading forum delete, and a clean uninstall that removes all
  field config/data and shipped config.
- Importer support: a source-agnostic migrate map table read by the
  separate entity_forum_importer module.
- Pre-release full code and security review completed 2026-07-19 (clean on
  SQL injection / XSS / CSRF; access model verified).
- Pre-release update hooks removed: no site installed the module before
  beta, so a fresh install gets the final schema and seed content directly
  from hook_schema() / hook_install(), with no historical update path.
