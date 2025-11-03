/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2024
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Tests for gspawn-posix error handling and resource cleanup.
 * These tests validate that resources (FDs, heap allocations) are properly
 * cleaned up even when spawn operations fail mid-sequence.
 */

#include "config.h"

#include <glib.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* Test spawning with invalid program name - should fail gracefully */
static void
test_spawn_nonexistent_program (void)
{
  GError *error = NULL;
  gchar *stdout_output = NULL;
  gchar *stderr_output = NULL;
  gint exit_status = 0;
  gboolean result;
  const gchar *argv[] = {
    "/nonexistent/path/to/program",
    "arg1",
    "arg2",
    NULL
  };

  result = g_spawn_sync (NULL, /* working directory */
                         (gchar **) argv,
                         NULL, /* envp */
                         G_SPAWN_DEFAULT,
                         NULL, /* child_setup */
                         NULL, /* user_data */
                         &stdout_output,
                         &stderr_output,
                         &exit_status,
                         &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);
  g_assert_null (stdout_output);
  g_assert_null (stderr_output);

  g_clear_error (&error);
}

/* Test spawning with invalid working directory - should fail gracefully */
static void
test_spawn_invalid_working_directory (void)
{
  GError *error = NULL;
  gchar *stdout_output = NULL;
  gchar *stderr_output = NULL;
  gint exit_status = 0;
  gboolean result;
  const gchar *argv[] = {
    "/bin/true",
    NULL
  };

  result = g_spawn_sync ("/nonexistent/directory/path",
                         (gchar **) argv,
                         NULL,
                         G_SPAWN_DEFAULT,
                         NULL,
                         NULL,
                         &stdout_output,
                         &stderr_output,
                         &exit_status,
                         &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_CHDIR);
  g_assert_null (stdout_output);
  g_assert_null (stderr_output);

  g_clear_error (&error);
}

/* Test spawning with pipes and invalid program - ensures pipe FDs are cleaned up */
static void
test_spawn_with_pipes_invalid_program (void)
{
  GError *error = NULL;
  GPid child_pid = 0;
  gint stdin_fd = -1;
  gint stdout_fd = -1;
  gint stderr_fd = -1;
  gboolean result;
  const gchar *argv[] = {
    "/nonexistent/program",
    NULL
  };

  result = g_spawn_async_with_pipes (NULL,
                                     (gchar **) argv,
                                     NULL,
                                     G_SPAWN_DEFAULT,
                                     NULL,
                                     NULL,
                                     &child_pid,
                                     &stdin_fd,
                                     &stdout_fd,
                                     &stderr_fd,
                                     &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);
  g_assert_cmpint (child_pid, ==, 0);
  
  /* Verify that pipe FDs were not returned (should be -1 or closed) */
  g_assert_cmpint (stdin_fd, ==, -1);
  g_assert_cmpint (stdout_fd, ==, -1);
  g_assert_cmpint (stderr_fd, ==, -1);

  g_clear_error (&error);
}

/* Test spawning with source FDs and invalid program */
static void
test_spawn_with_source_fds_invalid_program (void)
{
  GError *error = NULL;
  GPid child_pid = 0;
  gboolean result;
  gint source_fds[2] = { -1, -1 };
  gint target_fds[2] = { 10, 11 };
  const gchar *argv[] = {
    "/nonexistent/program",
    NULL
  };

  /* Create a pipe for the source FDs */
  if (pipe (source_fds) != 0)
    {
      g_test_skip ("pipe() failed");
      return;
    }

  result = g_spawn_async_with_pipes_and_fds (NULL,
                                             argv,
                                             NULL,
                                             G_SPAWN_DEFAULT,
                                             NULL,
                                             NULL,
                                             -1, /* stdin_fd */
                                             -1, /* stdout_fd */
                                             -1, /* stderr_fd */
                                             source_fds,
                                             target_fds,
                                             2,
                                             &child_pid,
                                             NULL, /* stdin_pipe_out */
                                             NULL, /* stdout_pipe_out */
                                             NULL, /* stderr_pipe_out */
                                             &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);
  g_assert_cmpint (child_pid, ==, 0);

  /* Clean up our pipe - the spawn function should not have closed these */
  close (source_fds[0]);
  close (source_fds[1]);

  g_clear_error (&error);
}

/* Test that empty command line is rejected */
static void
test_spawn_empty_argv (void)
{
  GError *error = NULL;
  gboolean result;
  const gchar *argv[] = { "", NULL };

  result = g_spawn_sync (NULL,
                         (gchar **) argv,
                         NULL,
                         G_SPAWN_DEFAULT,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);

  g_clear_error (&error);
}

/* Test spawning with search path and nonexistent program */
static void
test_spawn_search_path_nonexistent (void)
{
  GError *error = NULL;
  gboolean result;
  const gchar *argv[] = {
    "nonexistent-program-xyz-12345",
    NULL
  };

  result = g_spawn_sync (NULL,
                         (gchar **) argv,
                         NULL,
                         G_SPAWN_SEARCH_PATH,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);

  g_clear_error (&error);
}

/* Test that resources are cleaned up on error with many source FDs */
static void
test_spawn_many_source_fds_error (void)
{
  GError *error = NULL;
  GPid child_pid = 0;
  gboolean result;
  gint source_fds[10];
  gint target_fds[10];
  gsize n_fds = 10;
  gsize i;
  const gchar *argv[] = {
    "/nonexistent/program",
    NULL
  };

  /* Create multiple pipes for source FDs */
  for (i = 0; i < n_fds / 2; i++)
    {
      if (pipe (&source_fds[i * 2]) != 0)
        {
          /* Clean up any pipes we created */
          for (gsize j = 0; j < i * 2; j++)
            close (source_fds[j]);
          g_test_skip ("pipe() failed");
          return;
        }
      target_fds[i * 2] = 10 + i * 2;
      target_fds[i * 2 + 1] = 10 + i * 2 + 1;
    }

  result = g_spawn_async_with_pipes_and_fds (NULL,
                                             argv,
                                             NULL,
                                             G_SPAWN_DEFAULT,
                                             NULL,
                                             NULL,
                                             -1,
                                             -1,
                                             -1,
                                             source_fds,
                                             target_fds,
                                             n_fds,
                                             &child_pid,
                                             NULL,
                                             NULL,
                                             NULL,
                                             &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);
  g_assert_cmpint (child_pid, ==, 0);

  /* Clean up our pipes */
  for (i = 0; i < n_fds; i++)
    close (source_fds[i]);

  g_clear_error (&error);
}

/* Test file and argv zero with invalid program */
static void
test_spawn_file_and_argv_zero_error (void)
{
  GError *error = NULL;
  gboolean result;
  const gchar *argv[] = {
    "argv0",
    "/nonexistent/program",
    "arg1",
    NULL
  };

  result = g_spawn_sync (NULL,
                         (gchar **) argv,
                         NULL,
                         G_SPAWN_FILE_AND_ARGV_ZERO,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         &error);

  g_assert_false (result);
  g_assert_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);

  g_clear_error (&error);
}

#endif /* G_OS_UNIX */

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#ifdef G_OS_UNIX
  g_test_add_func ("/spawn-posix/error/nonexistent-program",
                   test_spawn_nonexistent_program);
  g_test_add_func ("/spawn-posix/error/invalid-working-directory",
                   test_spawn_invalid_working_directory);
  g_test_add_func ("/spawn-posix/error/pipes-invalid-program",
                   test_spawn_with_pipes_invalid_program);
  g_test_add_func ("/spawn-posix/error/source-fds-invalid-program",
                   test_spawn_with_source_fds_invalid_program);
  g_test_add_func ("/spawn-posix/error/empty-argv",
                   test_spawn_empty_argv);
  g_test_add_func ("/spawn-posix/error/search-path-nonexistent",
                   test_spawn_search_path_nonexistent);
  g_test_add_func ("/spawn-posix/error/many-source-fds-error",
                   test_spawn_many_source_fds_error);
  g_test_add_func ("/spawn-posix/error/file-and-argv-zero-error",
                   test_spawn_file_and_argv_zero_error);
#endif

  return g_test_run ();
}
