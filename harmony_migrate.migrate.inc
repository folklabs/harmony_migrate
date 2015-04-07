<?php
/**
 * @file harmony_migrate.migrate.inc
 */

/**
 * This is example code for a Drupal 6 to Drupal 7 migration. This won't actually
 * work without defining the referenced vocabularies, content types, and fields
 * on each side.
 */

/**
 * Implements hook_migrate_api().
 */
function harmony_migrate_migrate_api() {
  /**
   * Declare the api version and migration group.
   */
  $api = array(
    'api' => 2,
    'groups' => array(
      'forum' => array(
        'title' => t('Drupal 6 to Drupal 7 Harmony migrations.'),
      ),
      'migrations' => array(),
    ),
  );

  /**
   * Each migration being registered takes an array of arguments, some required
   * and some optional. Start with the common arguments required by all - the
   * source_connection (connection key, set up in settings.php, pointing to
   * the Drupal 6 database), source_version (major version of Drupal), and
   * group_name (a.k.a. import job).
   */
  $common_arguments = array(
    'source_connection' => 'legacy',
    'source_version' => 6,
    'group_name' => 'user',
  );

  // Register the user migration.
  // We just use the migrate_d2d D6 migration class as-is.
  $api['migrations']['User'] = $common_arguments + array(
    'description' => t('Migration of users from Drupal 6'),
    'class_name' => 'DrupalUser6Migration',
  );

  $common_arguments['group_name'] = 'forum';

  // For vocabulary migrations, source_vocabulary and destination_vocabulary are
  // required arguments. Note that in Drupal 6 vocabularies did not have machine
  // names, so we use the vocabulary ID to uniquely identify them.
  $vocabulary_arguments = array(
    'Topics' => array(
      'description' => t('Migration of Topics terms from Drupal 6'),
      'source_vocabulary' => '2',  // D6 Vocabulary ID
      'destination_vocabulary' => 'topics',
    ),
  );

  // Again, we're using the migrate_d2d class directly.
  // The soft dependency says that while we don't have to run the user migration
  // first, we want to make sure it's listed first so the vocabularies are
  // listed right ahead of the node migrations.
  // $common_vocabulary_arguments = $common_arguments + array(
  //   'class_name' => 'DrupalTerm6Migration',
  //   'soft_dependencies' => array('User'),
  // );
  // foreach ($vocabulary_arguments as $migration_name => $arguments) {
  //   $arguments += $common_vocabulary_arguments;
  //   $api['migrations'][$migration_name] = $arguments;
  // }

  // Node migrations - each has its own class derived from the migrate_d2d class,
  // specifying its particular field mappings and transformations. source_type
  // and destination_type are required arguments.
  $node_arguments = array(
    'topic' => array(
      'class_name' => 'ForumTopicMigration',
      'description' => t('Migration of article nodes from Drupal 6'),
      'source_type' => 'forum',
      'destination_type' => 'harmony_thread',
      // 'dependencies' => array('Image', 'Countries', 'Topics'),
    ),
    'post' => array(
      'class_name' => 'ForumPostMigration',
      'description' => t('Migration of article nodes from Drupal 6'),
      'source_type' => 'forum',
      'destination_type' => 'harmony_post',
      // 'dependencies' => array('Image', 'Countries', 'Topics'),
    ),
    'comment' => array(
      'class_name' => 'ForumCommentMigration',
      'description' => t(''),
      'source_type' => 'comment',
      'destination_type' => 'harmony_post',
      // 'dependencies' => array('Image', 'Countries', 'Topics'),
    ),
  );

  // Tell the node migrations where the users are coming from, so they can
  // set up the dependency and resolve D6->D7 uids.
  $common_node_arguments = $common_arguments + array(
      'user_migration' => 'User'
    );

  foreach ($node_arguments as $migration_name => $arguments) {
    $arguments = array_merge_recursive($arguments, $common_node_arguments);
    $api['migrations'][$migration_name] = $arguments;
  }

  return $api;
}