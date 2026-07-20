# Changelog - Entity Forum

Notable changes per release. The unreleased section accumulates change
summaries between pushes and doubles as the text for commit / release
descriptions.

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
