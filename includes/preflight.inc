<?php

/**
 * @file
 * Postflight and shutdown code.
 */

use Drush\Drush;

/**
 * The main Drush function.
 *
 * This function is still called by drush-launcher. It is no longer used by
 * Drush itself.
 */
function drush_main() {
  require dirname(__DIR__) . '/drush.php';
}

/**
 * We set this context to let the shutdown function know we reached the end of drush_main().
 *
 * @see drush_main()
 */
function drush_postflight() {
  drush_set_context("DRUSH_EXECUTION_COMPLETED", TRUE);
}

/**
 * Shutdown function for use while Drush and Drupal are bootstrapping and to return any
 * registered errors.
 *
 * The shutdown command checks whether certain options are set to reliably
 * detect and log some common Drupal initialization errors.
 *
 * If the command is being executed with the --backend option, the script
 * will return a json string containing the options and log information
 * used by the script.
 *
 * The command will exit with '1' if it was successfully executed, and the
 * result of drush_get_error() if it wasn't.
 */
function drush_shutdown() {
  // Avoid doing anything if our container has not been initialized yet.
  if (!Drush::hasContainer()) {
    return;
  }

  // Mysteriously make $user available during sess_write(). Avoids a NOTICE.
  global $user;

  if (!drush_get_context('DRUSH_EXECUTION_COMPLETED', FALSE) && !drush_get_context('DRUSH_USER_ABORT', FALSE)) {
    $php_error_message = '';
    if ($error = error_get_last()) {
      $php_error_message = "\n" . dt('Error: !message in !file, line !line', ['!message' => $error['message'], '!file' => $error['file'], '!line' => $error['line']]);
    }
    // We did not reach the end of the drush_main function,
    // this generally means somewhere in the code a call to exit(),
    // was made. We catch this, so that we can trigger an error in
    // those cases.
    drush_set_error("DRUSH_NOT_COMPLETED", dt("Drush command terminated abnormally due to an unrecoverable error.!message", ['!message' => $php_error_message]));
    // Attempt to give the user some advice about how to fix the problem
    _drush_postmortem();
  }

  if (Drush::backend()) {
    drush_backend_output();
  }

  // This way drush_return_status() will always be the last shutdown function (unless other shutdown functions register shutdown functions...)
  // and won't prevent other registered shutdown functions (IE from numerous cron methods) from running by calling exit() before they get a chance.
  register_shutdown_function('drush_return_status');
}

/**
 * Shutdown function to save code coverage data.
 */
function drush_coverage_shutdown() {
  if ($file_name = drush_get_context('DRUSH_CODE_COVERAGE', FALSE)) {
    $data = xdebug_get_code_coverage();
    xdebug_stop_code_coverage();

    // If coverage dump file contains anything, merge in the old data before
    // saving. This happens if the current drush command invoked another drush
    // command.
    if (file_exists($file_name) && $content = file_get_contents($file_name)) {
      $merge_data = unserialize($content);
      if (is_array($merge_data)) {
        foreach ($merge_data as $file => $lines) {
          if (!isset($data[$file])) {
            $data[$file] = $lines;
          }
          else {
            foreach ($lines as $num => $executed) {
              if (!isset($data[$file][$num])) {
                $data[$file][$num] = $executed;
              }
              else {
                $data[$file][$num] = ($executed == 1 ? $executed : $data[$file][$num]);
              }
            }
          }
        }
      }
    }

    file_put_contents($file_name, serialize($data));
  }
}

function drush_return_status() {
  // If a specific exit code was set, then use it.
  $exit_code = drush_get_context('DRUSH_EXIT_CODE');
  if (empty($exit_code)) {
    $exit_code = (drush_get_error()) ? DRUSH_FRAMEWORK_ERROR : DRUSH_SUCCESS;
  }

  exit($exit_code);
}
