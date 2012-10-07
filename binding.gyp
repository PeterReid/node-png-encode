{
  'targets': [
    {
      'target_name': 'node_png_encode',
      'sources': [
        'src/png_encode.cc'
      ],
      'dependencies': [
        'deps/lpng1513/binding.gyp:libpng',
        'deps/zlib-1.2.7/binding.gyp:zlib'
      ]
    }
  ]
}
