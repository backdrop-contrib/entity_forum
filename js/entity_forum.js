/**
 * @file
 * Entity Forum modal dialog glue for htmx.
 *
 * The per-post "Reply" links carry hx-get/hx-target attributes that load
 * the reply form fragment into #entity-forum-dialog-content; this file
 * opens/closes the native <dialog> around those swaps and keeps Backdrop
 * behaviors (notably the text-format WYSIWYG editor) working on swapped
 * content. Without JS the links navigate to the standalone form page.
 */
(function (Backdrop) {
  'use strict';

  Backdrop.behaviors.entityForumDialog = {
    attach: function (context) {
      var dialog = document.getElementById('entity-forum-dialog');
      if (!dialog || dialog.dataset.entityForumProcessed) {
        return;
      }
      dialog.dataset.entityForumProcessed = '1';
      var content = document.getElementById('entity-forum-dialog-content');

      // Old fragment content (e.g. a WYSIWYG instance) must be detached
      // cleanly before htmx replaces it with a new fragment.
      document.body.addEventListener('htmx:beforeSwap', function (event) {
        if (event.detail.target === content) {
          Backdrop.detachBehaviors(content);
        }
      });

      // Wire Backdrop behaviors onto the swapped-in form — this is what
      // initializes the text-format editor, whose libraries and settings
      // the topic page preloads — then open the dialog (a validation
      // error re-render swaps while it is already open).
      document.body.addEventListener('htmx:afterSwap', function (event) {
        if (event.detail.target === content) {
          Backdrop.attachBehaviors(content, Backdrop.settings);
          if (!dialog.open) {
            dialog.showModal();
          }
        }
      });

      // Sync WYSIWYG editors back into their textareas before htmx
      // serializes the form. filter.js does this on submit itself, but
      // only when the event was NOT default-prevented — and htmx
      // prevents it. Capture phase runs before htmx's own listener.
      document.addEventListener('submit', function (event) {
        if (content.contains(event.target)) {
          Backdrop.detachBehaviors(event.target, Backdrop.settings, 'serialize');
        }
      }, true);

      // Successful post: the server sends X-EntityForum-Redirect (not
      // HX-Redirect — a same-page #fragment redirect would only scroll,
      // never reload, leaving the dialog open and the new reply
      // invisible). Close, navigate, and force a reload when the path
      // is unchanged.
      document.body.addEventListener('htmx:afterRequest', function (event) {
        var xhr = event.detail.xhr;
        var redirect = xhr && xhr.getResponseHeader('X-EntityForum-Redirect');
        if (!redirect) {
          return;
        }
        if (dialog.open) {
          dialog.close();
        }
        var targetPath = redirect.split('#')[0];
        var currentPath = window.location.pathname + window.location.search;
        window.location.href = redirect;
        if (targetPath === '' || targetPath === currentPath) {
          window.location.reload();
        }
      });

      // Close button, and click-on-backdrop (a click on the <dialog>
      // element itself is outside its children).
      dialog.querySelector('.entity-forum-dialog-close').addEventListener('click', function () {
        dialog.close();
      });
      dialog.addEventListener('click', function (event) {
        if (event.target === dialog) {
          dialog.close();
        }
      });

      // Stale form fragments must not linger for the next open; detach
      // behaviors (destroys editor instances) before discarding.
      dialog.addEventListener('close', function () {
        Backdrop.detachBehaviors(content);
        content.innerHTML = '';
      });
    }
  };
})(Backdrop);
