#!/usr/bin/env python

import os

import locale
locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')

footer = """

# left out of UNIFIED_SOURCES for now; that's not C++ anyway, nothing else to unify it with
if not CONFIG['INTEL_ARCHITECTURE'] and CONFIG['CPU_ARCH'] == 'arm' and CONFIG['GNU_CC']:
    SOURCES += [
        'trunk/src/opts/memset.arm.S',
    ]

MSVC_ENABLE_PGO = True

FINAL_LIBRARY = 'gkmedias'
LOCAL_INCLUDES += [
    'trunk/include/config',
    'trunk/include/core',
    'trunk/include/effects',
    'trunk/include/gpu',
    'trunk/include/images',
    'trunk/include/lazy',
    'trunk/include/pathops',
    'trunk/include/pipe',
    'trunk/include/ports',
    'trunk/include/utils',
    'trunk/include/utils/mac',
    'trunk/include/utils/win',
    'trunk/include/views',
    'trunk/src/core',
    'trunk/src/gpu',
    'trunk/src/gpu/effects',
    'trunk/src/gpu/gl',
    'trunk/src/image',
    'trunk/src/lazy',
    'trunk/src/opts',
    'trunk/src/sfnt',
    'trunk/src/utils',
    'trunk/src/utils/android',
]

DEFINES['SK_A32_SHIFT'] = 24
DEFINES['SK_R32_SHIFT'] = 16
DEFINES['SK_G32_SHIFT'] = 8
DEFINES['SK_B32_SHIFT'] = 0

if CONFIG['MOZ_WIDGET_TOOLKIT'] in ('android', 'gtk2', 'gtk3', 'gonk', 'cocoa'):
    DEFINES['SK_USE_POSIX_THREADS'] = 1

if CONFIG['INTEL_ARCHITECTURE'] and CONFIG['HAVE_TOOLCHAIN_SUPPORT_MSSSE3']:
    DEFINES['SK_BUILD_SSSE3'] = 1

DEFINES['SK_FONTHOST_DOES_NOT_USE_FONTMGR'] = 1

"""

import json

platforms = ['linux', 'mac', 'android', 'win']

def generate_includes():
  includes = {}
  for root, dirs, files in os.walk('trunk/include'):
    for name in files:
      if name.endswith('.h'):
        includes[os.path.join(root, name)] = True

  return includes

def generate_platform_sources():
  sources = {}

  for plat in platforms:
    if os.system("cd trunk && GYP_GENERATORS=dump_mozbuild ./gyp_skia -D OS=%s gyp/skia_lib.gyp" % plat) != 0:
      print 'Failed to generate sources for ' + plat
      continue


    f = open('trunk/sources.json');
    sources[plat] = set(json.load(f));
    f.close()

  return sources


def generate_separated_sources(platform_sources):
  blacklist = [
    'SkImageDecoder_',
    '_gif',
    'SkFontConfigParser_android',
    'SkJpeg',
    'SkXML',
    'SkCity',
    'GrGLCreateNativeInterface',
    'fontconfig',
    '_neon',
    'SkImage_Codec',
    'SkBitmapChecksummer',
    'SkNativeGLContext',
    'SkFontConfig',
    'SkForceLinking',
    'SkMovie',
    'SkImageDecoder',
    'SkImageEncoder',
    'SkBitmapHasher',
  ]

  def isblacklisted(value):
    for item in blacklist:
      if value.find(item) >= 0:
        return True

    return False

  separated = {
    'common': {
      #'trunk/src/effects/gradients/SkGradientTileProc.cpp',
      'trunk/src/gpu/gl/GrGLCreateNativeInterface_none.cpp',
      'trunk/src/ports/SkDiscardableMemory_none.cpp',
      'trunk/src/ports/SkImageDecoder_empty.cpp',
      # 'trunk/src/images/SkImages.cpp',
      # 'trunk/src/images/SkImageRef.cpp',
      # 'trunk/src/images/SkImageRef_GlobalPool.cpp',
      # 'trunk/src/images/SkImageRefPool.cpp',
      # 'trunk/src/images/SkImageDecoder.cpp',
      # 'trunk/src/images/SkImageDecoder_Factory.cpp',
    },
    'android': {
      # 'trunk/src/ports/SkDebug_android.cpp',
      'trunk/src/ports/SkFontHost_cairo.cpp',
      # 'trunk/src/ports/SkFontHost_FreeType.cpp',
      # 'trunk/src/ports/SkFontHost_FreeType_common.cpp',
      # 'trunk/src/ports/SkThread_pthread.cpp',
      # 'trunk/src/ports/SkPurgeableMemoryBlock_android.cpp',
      # 'trunk/src/ports/SkTime_Unix.cpp',
      # 'trunk/src/utils/SkThreadUtils_pthread.cpp',
      # 'trunk/src/utils/SkThreadUtils_pthread_other.cpp',
      # 'trunk/src/images/SkImageRef_ashmem.cpp',
      # 'trunk/src/utils/android/ashmem.cpp',
    },
    'linux': {
      'trunk/src/ports/SkFontHost_cairo.cpp'
    },
    'intel': {
      'trunk/src/opts/SkXfermode_opts_none.cpp',
    },
    'arm': {
      'trunk/src/opts/opts_check_arm.cpp',
      'trunk/src/opts/SkBitmapProcState_opts_arm.cpp',
      'trunk/src/opts/SkBlitRow_opts_arm.cpp',
      'trunk/src/opts/SkBlitMask_opts_arm.cpp',
      'trunk/src/opts/SkXfermode_opts_arm.cpp',
    },
    'none': {
      'trunk/src/opts/SkXfermode_opts_none.cpp',
    }
  }

  for plat in platform_sources.keys():
    if not separated.has_key(plat):
      separated[plat] = set()

    for value in platform_sources[plat]:
      if isblacklisted(value):
        continue

      if value.find('_SSE') > 0 or value.find('_SSSE') > 0: #lol
        separated['intel'].add(value)
        continue

      if value.find('_arm') > 0 or value.find('_neon') > 0:
        separated['arm'].add(value)
        continue

      if value.find('_none') > 0:
        separated['none'].add(value)
        continue

      found = True
      for other in platforms:
        if other == plat or not platform_sources.has_key(other):
          continue

        if not value in platform_sources[other]:
          found = False
          break;

      if found:
        separated['common'].add(value)
      else:
        separated[plat].add(value)

  return separated



def write_list(f, name, values, indent):
  def write_indent(indent):
    for _ in range(indent):
        f.write(' ')

  val_list = sorted(map(lambda val: val.replace('../', 'trunk/'), values), key=lambda x: x.lower())

  if len(val_list) == 0:
    return

  write_indent(indent)
  f.write(name + ' += [\n')
  for val in val_list:
    write_indent(indent + 4)
    f.write('\'' + val + '\',\n')

  write_indent(4)
  f.write(']\n')

def write_mozbuild(includes, sources):
  filename = 'moz.build'
  f = open(filename, 'w')


  write_list(f, 'EXPORTS.skia', includes, 0)

  write_list(f, 'SOURCES', sources['common'], 0)

  f.write("if CONFIG['MOZ_WIDGET_TOOLKIT'] == 'android':\n")
  write_list(f, 'SOURCES', sources['android'], 4)

  f.write("if CONFIG['MOZ_WIDGET_TOOLKIT'] == 'cocoa':\n")
  write_list(f, 'SOURCES', sources['mac'], 4)

  f.write("if CONFIG['MOZ_WIDGET_GTK']:\n")
  write_list(f, 'SOURCES', sources['linux'], 4)

  f.write("if CONFIG['MOZ_WIDGET_TOOLKIT'] == 'windows':\n")
  write_list(f, 'SOURCES', sources['win'], 4)

  f.write("\n\n")
  f.write("if CONFIG['INTEL_ARCHITECTURE']:\n")
  write_list(f, 'SOURCES', sources['intel'], 4)

  f.write("elif CONFIG['CPU_ARCH'] == 'arm' and CONFIG['GNU_CC']:\n")
  write_list(f, 'SOURCES', sources['arm'], 4)

  f.write("else:\n")
  write_list(f, 'SOURCES', sources['none'], 4)

  f.write(footer)

  f.close()

  print 'Wrote ' + filename

def main():
  includes = generate_includes()
  platform_sources = generate_platform_sources()
  separated_sources = generate_separated_sources(platform_sources)
  write_mozbuild(includes, separated_sources)


if __name__ == '__main__':
  main()