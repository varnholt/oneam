#-------------------------------------------------
#
# Project created by QtCreator 2014-11-10T20:52:01
#
#-------------------------------------------------

QT       += core gui

# CONFIG += console

win32 {
   QTPLUGIN += plugins/qjpeg4
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = oneam
TEMPLATE = app

MOC_DIR = .moc
UI_DIR = .ui
OBJECTS_DIR = .obj

INCLUDEPATH += thirdparty/7z_C
INCLUDEPATH += thirdparty/fex
INCLUDEPATH += thirdparty/unrar
INCLUDEPATH += thirdparty/zlib
INCLUDEPATH += src


SOURCES += src/main.cpp\
    thirdparty/fex/Binary_Extractor.cpp \
    thirdparty/fex/blargg_common.cpp \
    thirdparty/fex/blargg_errors.cpp \
    thirdparty/fex/Data_Reader.cpp \
    thirdparty/fex/fex.cpp \
    thirdparty/fex/File_Extractor.cpp \
    thirdparty/fex/Gzip_Extractor.cpp \
    thirdparty/fex/Gzip_Reader.cpp \
    thirdparty/fex/Rar_Extractor.cpp \
    thirdparty/fex/Zip_Extractor.cpp \
    thirdparty/fex/Zip7_Extractor.cpp \
    thirdparty/fex/Zlib_Inflater.cpp \
    thirdparty/7z_C/7zAlloc.c \
    thirdparty/7z_C/7zBuf.c \
    thirdparty/7z_C/7zCrc.c \
    thirdparty/7z_C/7zDecode.c \
    thirdparty/7z_C/7zExtract.c \
    thirdparty/7z_C/7zHeader.c \
    thirdparty/7z_C/7zIn.c \
    thirdparty/7z_C/7zItem.c \
    thirdparty/7z_C/7zStream.c \
    thirdparty/7z_C/Bcj2.c \
    thirdparty/7z_C/Bra86.c \
    thirdparty/7z_C/LzmaDec.c \
    thirdparty/unrar/archive.cpp \
    thirdparty/unrar/arcread.cpp \
    thirdparty/unrar/coder.cpp \
    thirdparty/unrar/crc.cpp \
    thirdparty/unrar/encname.cpp \
    thirdparty/unrar/extract.cpp \
    thirdparty/unrar/getbits.cpp \
    thirdparty/unrar/model.cpp \
    thirdparty/unrar/rarvm.cpp \
    thirdparty/unrar/rarvmtbl.cpp \
    thirdparty/unrar/rawread.cpp \
    thirdparty/unrar/suballoc.cpp \
    thirdparty/unrar/unicode.cpp \
    thirdparty/unrar/unpack.cpp \
    thirdparty/unrar/unpack15.cpp \
    thirdparty/unrar/unpack20.cpp \
    thirdparty/unrar/unrar.cpp \
    thirdparty/unrar/unrar_misc.cpp \
    thirdparty/unrar/unrar_open.cpp \
    thirdparty/zlib/adler32.c \
    thirdparty/zlib/crc32.c \
    thirdparty/zlib/inffast.c \
    thirdparty/zlib/inflate.c \
    thirdparty/zlib/inftrees.c \
    thirdparty/zlib/zutil.c \
    src/unpacker.cpp \
    src/previewwidget.cpp \
    src/pagewidget.cpp \
    src/book.cpp \
    src/mainwidget.cpp \
    src/comicbookitem.cpp \
    src/comicconstants.cpp \
    src/configwidget.cpp \
    src/config.cpp

HEADERS  += \
    thirdparty/fex/Binary_Extractor.h \
    thirdparty/fex/blargg_common.h \
    thirdparty/fex/blargg_config.h \
    thirdparty/fex/blargg_endian.h \
    thirdparty/fex/blargg_errors.h \
    thirdparty/fex/blargg_source.h \
    thirdparty/fex/Data_Reader.h \
    thirdparty/fex/fex.h \
    thirdparty/fex/File_Extractor.h \
    thirdparty/fex/Gzip_Extractor.h \
    thirdparty/fex/Gzip_Reader.h \
    thirdparty/fex/Rar_Extractor.h \
    thirdparty/fex/Zip_Extractor.h \
    thirdparty/fex/Zip7_Extractor.h \
    thirdparty/fex/Zlib_Inflater.h \
    thirdparty/7z_C/7zAlloc.h \
    thirdparty/7z_C/7zBuf.h \
    thirdparty/7z_C/7zCrc.h \
    thirdparty/7z_C/7zDecode.h \
    thirdparty/7z_C/7zExtract.h \
    thirdparty/7z_C/7zHeader.h \
    thirdparty/7z_C/7zIn.h \
    thirdparty/7z_C/7zItem.h \
    thirdparty/7z_C/Bcj2.h \
    thirdparty/7z_C/Bra.h \
    thirdparty/7z_C/CpuArch.h \
    thirdparty/7z_C/LzmaDec.h \
    thirdparty/7z_C/Types.h \
    thirdparty/unrar/archive.hpp \
    thirdparty/unrar/array.hpp \
    thirdparty/unrar/coder.hpp \
    thirdparty/unrar/compress.hpp \
    thirdparty/unrar/encname.hpp \
    thirdparty/unrar/getbits.hpp \
    thirdparty/unrar/headers.hpp \
    thirdparty/unrar/model.hpp \
    thirdparty/unrar/rar.hpp \
    thirdparty/unrar/rarvm.hpp \
    thirdparty/unrar/rawread.hpp \
    thirdparty/unrar/suballoc.hpp \
    thirdparty/unrar/unicode.hpp \
    thirdparty/unrar/unpack.hpp \
    thirdparty/unrar/unrar.h \
    thirdparty/zlib/crc32.h \
    thirdparty/zlib/inffast.h \
    thirdparty/zlib/inffixed.h \
    thirdparty/zlib/inflate.h \
    thirdparty/zlib/inftrees.h \
    thirdparty/zlib/zconf.h \
    thirdparty/zlib/zlib.h \
    thirdparty/zlib/zutil.h \
    src/unpacker.h \
    src/previewwidget.h \
    src/pagewidget.h \
    src/book.h \
    src/mainwidget.h \
    src/comicbookitem.h \
    src/comicconstants.h \
    src/configwidget.h \
    src/config.h

FORMS += \
    src/previewwidget.ui \
    src/pagewidget.ui \
    src/mainwidget.ui \
    src/configwidget.ui

OTHER_FILES += \
    oneam.ini

