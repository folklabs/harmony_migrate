<?php

define("LEGACY_DATABASE", 'legacy');

// Debug helper to test migrating one topic
define("TOPIC_ID", 6567);


/**
 * Creates Harmony threads based on Forum topics.
 */
class ForumTopicMigration extends Migration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $query = Database::getConnection('default', LEGACY_DATABASE)
                        ->select('node', 'n');
    $query->innerJoin('node_revisions', 'nr', 'n.vid=nr.vid');
    $query->leftJoin('forum', 'f', 'n.vid=f.vid');
    $query->fields('n', array('nid','vid', 'uid', 'created', 'changed'));
    $query->fields('nr', array('title'));
    $query->condition('n.type', 'forum');

    //TODO: debug hack!
    $query->condition('n.nid', TOPIC_ID);

    // Set source and destination.
    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));
    $this->destination = new MigrateDestinationEntityAPI(
      'harmony_thread',  // Entity type
      'harmony_thread'   // bundle
    );
    // Set up database maping.
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'nid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'description' => 'D6 Unique Node ID',
          'alias' => 'n',
        )
      ),
      MigrateDestinationEntityAPI::getKeySchema('harmony_thread')
    );

    $this->addFieldMapping('thread_id', 'nid');
    $this->addFieldMapping('uid', 'uid');
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('created', 'created');
    $this->addFieldMapping('updated', 'changed');
    $this->addFieldMapping('field_harmony_post_count')->defaultValue(0);

    $this->addUnmigratedDestinations(array('status', 'type', 'locked', 'hidden',
      'pinned', 'favourites', 'likes', 'views', 'field_harmony_thread_cont_from'));
  }
}


/**
 * Creates the first Harmony post of a thread based on the Forum topic.
 */
class ForumPostMigration extends Migration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $query = Database::getConnection('default', LEGACY_DATABASE)
                        ->select('node', 'n');
    $query->innerJoin('node_revisions', 'nr', 'n.vid=nr.vid');
    $query->leftJoin('forum', 'f', 'n.vid=f.vid');
    $query->fields('n', array('nid', 'vid', 'uid', 'title', 'created', 'changed'));
    $query->fields('nr', array('body'));
    $query->condition('n.type', 'forum');

    //TODO: debug hack!
    $query->condition('n.nid', TOPIC_ID);

    // Set source and destination.
    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));
    $this->destination = new MigrateDestinationEntityAPI(
      'harmony_post',  // Entity type
      'harmony_post'   // bundle
    );
    // Set up database maping.
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'nid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'description' => 'D6 Unique Node ID',
          'alias' => 'n',
        )
      ),
      MigrateDestinationEntityAPI::getKeySchema('harmony_post')
    );

    $this->addFieldMapping('post_id', 'nid');
    $this->addFieldMapping('uid', 'uid');
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('created', 'created');
    $this->addFieldMapping('updated', 'changed');
    $this->addFieldMapping('field_harmony_text', 'body');
    $this->addFieldMapping('first_post')
      ->defaultValue('1');

    $this->addFieldMapping('field_harmony_thread', 'nid')
      ->sourceMigration('topic');

    $this->addUnmigratedDestinations(array('status', 'locked', 'hidden',
      'hostname', 'log_show'));
  }

  protected function createStub($migration, $source_id) {
    $entity_type = 'harmony_post';
    $entity = entity_create($entity_type, array('type' => 'harmony_post'));
    $wrapper = entity_metadata_wrapper($entity_type, $entity);
    $wrapper->title = t('Stub for @id', array('@id' => $source_id));
    $wrapper->status = 1;

    $wrapper->save();

    return array($entity->post_id);
  }
}


/**
 * Creates Harmony reply posts based on the Forum comments.
 */
class ForumCommentMigration extends Migration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $query = Database::getConnection('default', LEGACY_DATABASE)
                        ->select('comments', 'c');
    $query->leftJoin('node', 'n', 'n.nid=c.nid');
    $query->fields('c', array('cid', 'nid', 'uid', 'subject', 'comment', 'timestamp', 'pid'));
    $query->fields('n', array('type', 'title'));
    $query->condition('n.type', 'forum');

    //TODO: debug hack!
    $query->condition('n.nid', TOPIC_ID);

    // Set source and destination.
    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));
    $this->destination = new MigrateDestinationEntityAPI(
      'harmony_post',  // Entity type
      'harmony_post'   // bundle
    );
    // Set up database maping.
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'cid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'description' => 'D6 Unique Node ID',
          'alias' => 'c',
        )
      ),
      MigrateDestinationEntityAPI::getKeySchema('harmony_post')
    );

    $this->addFieldMapping('post_id', 'cid');
    $this->addFieldMapping('uid', 'uid');
    // The subject is migrated to title, but title is not visible by default in harmony_post.
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('created', 'timestamp');
    $this->addFieldMapping('field_harmony_text', 'comment');

    $this->addFieldMapping('field_harmony_thread', 'nid')
      ->sourceMigration('topic');

    $this->addFieldMapping('field_harmony_post_is_reply_to', 'nid');
      // ->sourceMigration('post');

    $this->addUnmigratedDestinations(array('status', 'locked', 'hidden',
      'hostname', 'log_show', 'updated', 'log', 'revision'));
  }

  function prepare($entity, stdClass $row) {
    $post_parent_id = 0;

    // Change the parent post ID if the original comment had a parent comment ID
    if ($row->pid != 0) {
      $post_parent_id = $this->handleSourceMigration(array('comment'), $row->pid);
    }
    else {
      $post_parent_id = $this->handleSourceMigration(array('post'), $row->nid);
    }
    $entity->field_harmony_post_is_reply_to['und'][0]['target_id'] = $post_parent_id['destid1'];
  }

  protected function createStub($migration, $source_id) {
    $entity_type = 'harmony_post';
    $entity = entity_create($entity_type, array('type' => 'harmony_post'));
    $wrapper = entity_metadata_wrapper($entity_type, $entity);
    $wrapper->title = t('Stub for @id', array('@id' => $source_id));
    $wrapper->status = 1;
    $wrapper->uid = 1;

    $wrapper->save();

    return array($entity->post_id);
  }
}


