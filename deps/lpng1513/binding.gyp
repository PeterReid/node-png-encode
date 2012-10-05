{
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
          },
        },
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
          },
        },
      }
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
      },
      'VCLibrarianTool': {
      },
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
      },
    },
    'conditions': [
      ['OS == "win"', {
        'defines': [
          'WIN32'
        ],
      }]
    ],
  },

  'targets': [
    {
      'target_name': 'libpng',
      'type': 'static_library',
      'include_dirs': [ '.', '../zlib-1.2.7' ],
      'direct_dependent_settings': {
        'include_dirs': [ '.' ],
        'defines': [
        ],
      },
      'defines': [
      ],
      'sources': [ 
        './png.c',
        './pngwrite.c',
        './pngset.c',
        './pngerror.c',
        './pngget.c',
        './pngmem.c',
        './pngwutil.c',
        './pngtrans.c',
        './pngwio.c',
        './pngwtran.c'
      ],
    },
  ]
}
