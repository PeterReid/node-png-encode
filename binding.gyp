{
  'targets': [
    {
      'target_name': 'node_sqlite3',
      'sources': [
        'src/blobber.cc',
        'src/node_sqlite3.cc'
      ],
      'dependencies': [
        'deps/giflib-5.0.0/binding.gyp:giflib'
      ]
    }
  ]
}
