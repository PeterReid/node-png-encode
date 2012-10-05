{
  'targets': [
    {
      'target_name': 'node_gifblobber',
      'sources': [
        'src/blobber.cc'
      ],
      'dependencies': [
        'deps/giflib-5.0.0/binding.gyp:giflib'
      ]
    }
  ]
}
