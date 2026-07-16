# Entity Forum

**Entity Forum** is a forum system for Backdrop CMS built on custom entities
for forums, topics, and replies (similar to bbPress). It is designed to run on
any Backdrop site and to migrate content from an existing WordPress bbPress
install via the companion `entity_forum_importer` module.

---

## Key Features

- **Three fieldable entity types** on Entity Plus: `entity_forum_forum`,
  `entity_forum_topic`, and `entity_forum_reply`. Because they are fieldable,
  sites can attach their own fields (attachments, images, ratings, …) through
  the normal Backdrop UI at **Structure → Forums**
  (`admin/structure/entity-forum`) with no forum-specific code.
- **Frontend**: a forums index, per-forum topic lists (a site-builder-editable
  View), and topic pages with threaded replies and an inline reply form.
- **Sub-forums** with rolled-up post counts and climbing breadcrumbs.
- **Views integration**: all three entity types are Views base tables, with
  extra fields for author picture, aggregate forum counts, and last activity.
- **Member deletion protection**: forum content survives account deletion with
  full attribution (author-name snapshot; author IDs are preserved and
  reinstatable).
- **Forum pictures**: a module-owned avatar per member (with an admin-set
  default), shown on posts and in the topic list.
- **Attachments**: a `field_attachments` file field on topics and replies;
  imported bbPress attachments are also rendered (login-gated).

---

## Installation & Setup

1. Enable the `entity_forum` module and its dependencies (`entity`,
   `entity_plus`, `views`).
2. Visit **Administration → Content → Forums**
   (`admin/content/entity-forum`) to create forums.
3. Set the default forum picture and other options at
   **Content → Forums → Settings**
   (`admin/content/entity-forum/settings`).
4. Grant the forum permissions to the appropriate roles at
   **Configuration → People → Permissions**.

---

## Permissions

Out of the box the module ships role-based permissions:

- **Create Entity Forum topics / replies** — post new content, and edit or
  delete the user's *own* posts.
- **Administer Entity Forum topics / replies** — moderator capability: edit or
  delete *any* post.
- **Administer Entity Forum** / **View private Entity Forums** — manage forums
  and see private forums.

### Granular, per-field permissions (Field Permissions)

**To limit who can see or set a field, use the contrib
[Field Permissions](https://backdropcms.org/project/field_permissions)
module.** Because the forum entity types are fieldable, it lets you control
access **per field**: edit a field (an attachment field, say, or a
classified ad's price) and set it to *Custom permissions*, and Field
Permissions automatically adds create / edit / edit-own / view / view-own
permissions for that field to the permissions page — so you can, for
example, let only certain roles add attachments, or show an offer amount
only to the seller and the moderators.

> **Not Content Access.** Content Access is a *node* access module — it
> works through the node access system, and Entity Forum's forums, topics
> and replies are not nodes, so it has no effect on them. Field
> Permissions is the module that applies here.

Entity Forum bridges its author information so the *"own"* variants work
correctly on forum posts (its `hook_entity_load()` exposes the post author as a
standard `uid` property, which Field Permissions uses to determine ownership).
Field Permissions is **optional** — fields remain publicly editable (subject to
the normal post permissions above) until you opt a field into custom mode, so
existing fields never lock themselves out.

---

## Related Modules

- **entity_forum_importer** — imports bbPress content from a WordPress database
  into Entity Forum.
- **Field Permissions** (contrib, optional) — per-field permissions, as above.
