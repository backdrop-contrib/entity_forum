/**
 * @file
 * Entity Forum modal dialog glue for htmx.
 *
 * The per-post "Reply" links carry hx-get/hx-target attributes that load
 * the reply form fragment into #entity-forum-dialog-content; this file
 * opens/closes the native <dialog> around those swaps. Without JS the
 * links navigate to the standalone form page instead.
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

      // Open the dialog whenever htmx swaps content into it (reply link
      // clicked, or a validation-error re-render while already open).
      document.body.addEventListener('htmx:afterSwap', function (event) {
        if (event.detail.target && event.detail.target.id === 'entity-forum-dialog-content' && !dialog.open) {
          dialog.showModal();
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

      // Stale form fragments must not linger for the next open.
      dialog.addEventListener('close', function () {
        document.getElementById('entity-forum-dialog-content').innerHTML = '';
      });
    }
  };
})(Backdrop);
