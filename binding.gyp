{
  'targets': [
    {
      'target_name': 'node_gifblobber',
      'sources': [
        'src/blobber.cc'
      ],
      'dependencies': [
        'deps/lpng1513/binding.gyp:libpng',
        'deps/zlib-1.2.7/binding.gyp:zlib'
      ]
    }
  ]
}
