# Entity Forum

**Entity Forum** is a forum system for Backdrop CMS built on custom entities
for forums, topics, and replies — in the vein of WordPress bbPress, or
Backdrop's own Forum and Forum NG modules.

It runs on any Backdrop site, and existing forum content can be migrated in via
the companion `entity_forum_importer` module: **WordPress bbPress** is
supported today, with **Backdrop Forum**, **Forum NG** and **comments**
converters in development.

---

## Why Entity Forum

**It is built out of the tools you already know.** Forums, topics and replies
are real entities, so they are fieldable, Views-able and permission-able
through the normal Backdrop UI. There is no forum-shaped walled garden: if you
can build a content type and a View, you can shape the forum.

**It ships primitives, not policy.** The module deliberately does not decide
what your forum is for. It gives you the pieces — types, fields, views, access
— and stays out of the way, rather than shipping one site's idea of a forum and
asking you to fight it.

---

## Key Features

- **Three fieldable entity types**: `entity_forum_forum`, `entity_forum_topic`
  and `entity_forum_reply`. Attach your own fields (images, attachments,
  ratings, prices, …) through the normal Backdrop UI, with no forum-specific
  code.
- **Topic types**, like content types: each has its own fields, and a forum is
  set to one — so a Classified Adverts forum can carry a price and photos while
  the discussion forums stay plain. Replies inherit their topic's type, so a
  reply to an ad can have its own fields too.
- **Sub-forums** with rolled-up post counts and climbing breadcrumbs.
- **Threaded replies**, paged so a conversation is never split across pages.
- **Private forums**: gated by permission, with topics, replies and file
  attachments all inheriting it.
- **Moderation**: hold a member's posts for review or suspend them from posting;
  held posts queue for approval. Members can report posts, and a reported post
  stays visible with a moderator-only highlight — a human decides.
- **A replacement for core comments**: opt a content type in and its nodes get
  a forum discussion instead, with the same replies, moderation and fields.
- **Views integration**: all three entity types are Views base tables, with
  extra fields for author picture, aggregate forum counts and last activity.
  The shipped views are meant to be **copied and amended** — and each forum
  picks which view display renders its topic list, so a classified-ads forum
  can show an image grid while the discussion forums stay a topic table.
- **Attachments are just fields**: add a file or image field and that *is* your
  attachment system — widget, validation, image styles and per-field
  permissions all included. The module adds only the part fields cannot do:
  files belonging to a **private** forum's posts are kept in the private file
  system and served only to members who may view that forum.
- **Per-field permissions** via the contrib
  [Field Permissions](https://backdropcms.org/project/field_permissions)
  module — let only certain roles add attachments, or show an offer amount only
  to the seller and the moderators. Entity Forum bridges its author information
  so the *own*-content variants work correctly on forum posts.
- **Member deletion protection**: forum content survives account deletion with
  full attribution — a member leaving does not blank their history or orphan
  the threads they started.
- **Forum pictures**: a module-owned avatar per member, with an admin-set
  default, shown on posts and in topic lists.

---

## Requirements

Backdrop CMS with the `entity`, `entity_plus` and `views` modules.

Entity Forum provides the `forums`, `forum/*` and `topic/*` paths and replaces
the core-style forum modules, so it **cannot run alongside `forum` or
`forum_ng`** — installation is blocked while either is enabled.

---

## Limitations

**One source forum per site.** This module and its importers support migrating
from *one* forum system; you cannot combine two different forums into a single
Backdrop Entity Forum. This is such a fringe case that we decided against
allowing multiple forum imports. If it is something you require, please submit
an issue — charges may apply for such development.

**Inline images stay public.** Images placed directly into post text are part
of the content, so their URLs cannot be gated by forum privacy. Private-forum
*attachments* (file and image fields) are gated.

---

## Related Modules

- **entity_forum_importer** — imports existing forum content into Entity Forum.
  WordPress bbPress is the implemented source; converters for Backdrop's own
  `forum`, `forum_ng` and comments are planned alongside it. Disposable by
  design: remove it once a site's migration is done.
- **Field Permissions** (contrib, optional) — per-field permissions, as above.
