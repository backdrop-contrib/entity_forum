# Entity Forum

**Entity Forum** is a forum system for Backdrop CMS built with custom entities
for forums, topics, and replies, all fieldable.

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
what your forum is for. It gives you the pieces - types, fields, views, access,
and stays out of the way, rather than shipping one site's idea of a forum and
asking you to fight it.

---

## Key Features

- **Three fieldable entity types**: `entity_forum_forum`, `entity_forum_topic`
  and `entity_forum_reply`. Attach your own fields (images, attachments,
  ratings, prices, …) through the normal Backdrop UI, with no forum-specific
  code.
- **Topic types**, like content types: each has its own fields, and a forum is
  set to one, so a Classified Adverts forum can carry a price and photos while
  the discussion forums stay plain. Replies inherit their topic's type, so a
  reply to an ad can have its own fields too.
- **Sub-forums** with rolled-up post counts and climbing breadcrumbs.
- **Threaded replies**, paged so a conversation is never split across pages.
- **Private forums**: gated by permission, with topics, replies and file
  attachments all inheriting it.
- **Moderation**: hold a member's posts for review or suspend them from posting;
  held posts queue for approval. Members can report posts, and a reported post
  stays visible with a moderator-only highlight, a human decides.
- **A replacement for core comments**: opt a content type in and its nodes get
  a forum discussion instead, with the same replies, moderation and fields.
- **Views integration**: all three entity types are Views base tables, with
  extra fields for author picture, aggregate forum counts and last activity.
  The shipped views are meant to be **copied and amended**, and each forum
  picks which view display renders its topic list, so a classified-ads forum
  can show an image grid while the discussion forums stay a topic table.
- **Attachments are just fields**: add a file or image field and that *is* your
  attachment system, widget, validation, image styles and per-field
  permissions all included. The module adds only the part fields cannot do:
  files belonging to a **private** forum's posts are kept in the private file
  system and served only to members who may view that forum.
- **Per-field permissions** via the contrib
  [Field Permissions](https://backdropcms.org/project/field_permissions)
  module, let only certain roles add attachments, or show an offer amount only
  to the seller and the moderators. Entity Forum bridges its author information
  so the *own*-content variants work correctly on forum posts.
- **Member deletion protection**: forum content survives account deletion with
  full attribution, a member leaving does not blank their history or orphan
  the threads they started.
- **Forum pictures**: a module-owned avatar per member, with an admin-set
  default, shown on posts and in topic lists.
- **Removable starter content**: a fresh install seeds a plain discussion
  forum and a worked classified-ads section (fielded topic type, offer-amount
  reply field, ad grid view) so the module demonstrates itself out of the box.
  One click on the settings page removes the demo forums; the classified type,
  its fields and the grid view stay behind as reusable building blocks.

---

## Requirements

- Backdrop CMS 1.x
- PHP 8.0+
- Entity
- Entity Plus
- Views
- Text (core)

---

## Installation

Install this module using the official Backdrop CMS instructions at https://docs.backdropcms.org/documentation/extend-with-modules

Enable the module, then clear your system caches to register the admin menu items.

---

## Configuration

To add or modify fields on Forums, Topics or Replies, visit the "Forum
structure" page (Structure → Forum structure, admin/structure/entity-forum).

For general administration: Content → Forums (admin/content/entity-forum).

The three overview tabs (Forums, Topics, Replies) are each a shipped View —
the columns below are the defaults, and every one is customisable in the
Views UI.

### Tabs: Forums | Topics | Replies | Settings | Help

**Forums** - list of forums (admin/content/entity-forum)
- Add forum
- Visibility
- Topic count
- Status
- Listed status
- Operations [edit | delete]

**Topics** - list of topics, with search (admin/content/entity-forum/topics)
- Title
- Type
- Forum
- Author
- Replies
- Last active
- Listing
- Status
- Operations [edit | delete]

**Replies** - list of replies (admin/content/entity-forum/replies)
- Content
- Topic
- Author
- Created
- Status
- Operations [edit | delete]

**Settings** (admin/content/entity-forum/settings)
- Default forum picture
- Content discussions (comments replacement), see below
- Remove pre-installed forums (shown while the starter demo forums are still present)

**Help** (admin/content/entity-forum/help)

**Content discussions** (comments replacement) (admin/content/entity-forum/settings/discussions/add)
- Content type, the content whose discussion appears below it. Remember to disable core comments on this type yourself.
- Discussion forum, the forum the discussion topics are created in.
- Show on [Every item of this type | Only items the author enables]

---

## Limitations

**One source forum per site.** This module and its importers support migrating
from *one* forum system; you cannot combine two different forums into a single
Backdrop Entity Forum. This is such a fringe case that we decided against
allowing multiple forum imports. If it is something you require, please submit
an issue, charges may apply for such development.

Entity Forum provides the `forums`, `forum/*` and `topic/*` paths, so it
**cannot run alongside `forum` or `forum_ng`**, installation is blocked
while either is enabled.

**Inline images stay public.** Images placed directly into post text are part
of the content, so their URLs cannot be gated by forum privacy. Private-forum
*attachments* (file and image fields) are gated.

**CSS styling.** Initial CSS is in place, but there will be gaps where some
fields or markup do not yet receive styling. If you would like a specific
class or hook added, please raise an issue in the GitHub issue queue.

---

## Related / Useful Modules

- **entity_forum_importer**, imports existing forum content into Entity Forum.
  WordPress bbPress is the implemented source; converters for Backdrop's own
  `forum`, `forum_ng` and comments are planned alongside it. Disposable by
  design: remove it once a site's migration is done.
- **Field Permissions** (contrib, optional), per-field permissions, as above.
- **[Image Library Image Access](https://backdropcms.org/project/image_library_image_access)**
  (contrib, optional), when forum members can upload images through the
  editor, the core image library dialog lets them browse every image on the
  site; this module limits users to their own uploads (plus a bypass
  permission). Entity Forum's status report recommends it when a text format
  allows editor image uploads.

---

## Planned Features

- Backdrop Forum importer (the contrib `forum` module)
- Forum NG importer
- Comments importer
- Notifications sub-module, optional email on new topics/replies (and, for
  moderators, held posts and reports)
- Frontend help pages for moderators and members (the current Help tab is
  admin-only)

---

## Issues

Bugs and feature requests should be reported in the Issue Queue: https://github.com/backdrop-contrib/entity_forum/issues

---

## Current Maintainer(s)
- Steve Moorhouse (albanycomputers) (https://github.com/albanycomputers)
- Additional maintainers and contributors welcome.

---

## Credits
- Steve Moorhouse - Zulip (DrAlbany)
- Assisted by AI.

- Current development is sponsored by [Albany Computer Services](https://www.albany-computers.co.uk), providers of computer support, [web design](https://www.albanywebdesign.co.uk), and [web hosting](https://www.albany-hosting.co.uk).

## License
This project is GPL v2 or later software. See the LICENSE.txt file in this directory for complete text.
