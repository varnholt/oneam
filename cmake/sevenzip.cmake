# Adapted from Microsoft vcpkg, ports/7zip/CMakeLists.txt, revision
# 6fdd23fa0a9479a9f9a1c48178696f60cf39255d. License: assets/licenses/vcpkg.txt. Upstream sources are
# unmodified; use portable C implementations and export CreateObject for normal linker integration
# with the shared decoder.

# Starting points: DOC/readme.txt
#
# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Alone2 : 7-Zip console executable that supports all
# formats (but skip the bits for the executable)
#
# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Alone2/makefile : nmake (MSVC) ->
# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Format7zF/Arc.mak ->
# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/7zip.mak
#
# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Alone2/(makefile.gcc|../../cmpl_<variant>.mak) : other ->
# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Format7zF/Arc_gcc.mak ->
# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/7zip_gcc.mak

option(USE_NO_ASM "Disable ASM")

add_library(7zip SHARED)

if(MSVC)
    if(NOT USE_NO_ASM AND VCPKG_TARGET_ARCHITECTURE MATCHES "x86|x64")
        enable_language(ASM_MASM)
    endif()
else()
    if(VCPKG_TARGET_ARCHITECTURE MATCHES "arm64")
        enable_language(ASM)
    endif()
endif()
get_cmake_property(enabled_languages ENABLED_LANGUAGES)

# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Format7zF/Arc[_gcc].mak

target_sources(
    7zip
    PRIVATE # COMPRESS_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Bcj2Coder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Bcj2Register.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BcjCoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BcjRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BitlDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BranchMisc.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BranchRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/ByteSwap.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BZip2Crc.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BZip2Decoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BZip2Encoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/BZip2Register.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/CopyCoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/CopyRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Deflate64Register.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/DeflateDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/DeflateEncoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/DeflateRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/DeltaFilter.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/ImplodeDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzfseDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzhDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Lzma2Decoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Lzma2Encoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Lzma2Register.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzmaDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzmaEncoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzmaRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzmsDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzOutWindow.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/LzxDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/PpmdDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/PpmdEncoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/PpmdRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/PpmdZip.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/QuantumDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Rar1Decoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Rar2Decoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Rar3Decoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Rar3Vm.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/Rar5Decoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/RarCodecsRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/ShrinkDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/XpressDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/XzDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/XzEncoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/ZDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/ZlibDecoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/ZlibEncoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/ZstdDecoder.cpp
            # CRYPTO_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/7zAes.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/7zAesRegister.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/HmacSha1.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/HmacSha256.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/MyAes.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/MyAesReg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/Pbkdf2HmacSha1.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/RandGen.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/Rar20Crypto.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/Rar5Aes.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/RarAes.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/WzAes.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/ZipCrypto.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Crypto/ZipStrong.cpp
            # C_OBJS
            ${sevenzip_SOURCE_DIR}/C/7zBuf2.c
            ${sevenzip_SOURCE_DIR}/C/7zStream.c
            ${sevenzip_SOURCE_DIR}/C/Alloc.c
            ${sevenzip_SOURCE_DIR}/C/Bcj2.c
            ${sevenzip_SOURCE_DIR}/C/Bcj2Enc.c
            ${sevenzip_SOURCE_DIR}/C/Blake2s.c
            ${sevenzip_SOURCE_DIR}/C/Bra.c
            ${sevenzip_SOURCE_DIR}/C/Bra86.c
            ${sevenzip_SOURCE_DIR}/C/BraIA64.c
            ${sevenzip_SOURCE_DIR}/C/BwtSort.c
            ${sevenzip_SOURCE_DIR}/C/CpuArch.c
            ${sevenzip_SOURCE_DIR}/C/Delta.c
            ${sevenzip_SOURCE_DIR}/C/HuffEnc.c
            ${sevenzip_SOURCE_DIR}/C/LzFind.c
            ${sevenzip_SOURCE_DIR}/C/LzFindMt.c
            ${sevenzip_SOURCE_DIR}/C/Lzma2Dec.c
            ${sevenzip_SOURCE_DIR}/C/Lzma2DecMt.c
            ${sevenzip_SOURCE_DIR}/C/Lzma2Enc.c
            ${sevenzip_SOURCE_DIR}/C/LzmaDec.c
            ${sevenzip_SOURCE_DIR}/C/LzmaEnc.c
            ${sevenzip_SOURCE_DIR}/C/Md5.c
            ${sevenzip_SOURCE_DIR}/C/MtCoder.c
            ${sevenzip_SOURCE_DIR}/C/MtDec.c
            ${sevenzip_SOURCE_DIR}/C/Ppmd7.c
            ${sevenzip_SOURCE_DIR}/C/Ppmd7aDec.c
            ${sevenzip_SOURCE_DIR}/C/Ppmd7Dec.c
            ${sevenzip_SOURCE_DIR}/C/Ppmd7Enc.c
            ${sevenzip_SOURCE_DIR}/C/Ppmd8.c
            ${sevenzip_SOURCE_DIR}/C/Ppmd8Dec.c
            ${sevenzip_SOURCE_DIR}/C/Ppmd8Enc.c
            ${sevenzip_SOURCE_DIR}/C/Sha3.c
            ${sevenzip_SOURCE_DIR}/C/Sha512.c
            ${sevenzip_SOURCE_DIR}/C/Sha512Opt.c
            ${sevenzip_SOURCE_DIR}/C/SwapBytes.c
            ${sevenzip_SOURCE_DIR}/C/Threads.c
            ${sevenzip_SOURCE_DIR}/C/Xxh64.c
            ${sevenzip_SOURCE_DIR}/C/Xz.c
            ${sevenzip_SOURCE_DIR}/C/XzDec.c
            ${sevenzip_SOURCE_DIR}/C/XzEnc.c
            ${sevenzip_SOURCE_DIR}/C/XzIn.c
            ${sevenzip_SOURCE_DIR}/C/ZstdDec.c
            # COMMON_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/Common/CRC.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/CrcReg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/DynLimBuf.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/IntToString.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/LzFindPrepare.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Md5Reg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/MyMap.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/MyString.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/MyVector.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/MyXml.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/NewHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Sha1Reg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Sha256Reg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Sha3Reg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Sha512Reg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Sha512Prepare.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/StringConvert.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/StringToInt.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/UTFConvert.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Wildcard.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/Xxh64Reg.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/XzCrc64Init.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Common/XzCrc64Reg.cpp
            # 7ZIP_COMMON_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/CreateCoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/CWrappers.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/InBuffer.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/InOutTempBuffer.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/FilterCoder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/LimitedStreams.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/LockedStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/MemBlocks.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/MethodId.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/MethodProps.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/OffsetStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/OutBuffer.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/OutMemStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/ProgressMt.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/ProgressUtils.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/PropId.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/StreamBinder.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/StreamObjects.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/StreamUtils.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/UniqBlocks.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Common/VirtThread.cpp
            # WIN_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/Windows/FileDir.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/FileFind.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/FileIO.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/FileName.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/PropVariant.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/PropVariantConv.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/PropVariantUtils.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/Synchronization.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/System.cpp
            ${sevenzip_SOURCE_DIR}/CPP/Windows/TimeUtils.cpp
            # AR_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ApfsHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ApmHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ArHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ArjHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Base64Handler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Bz2Handler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ComHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/CpioHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/CramfsHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/DeflateProps.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/DmgHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ElfHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ExtHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/FatHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/FlvHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/GzHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/GptHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/HandlerCont.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/HfsHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/IhexHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/LzhHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/LzmaHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/MachoHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/MbrHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/MslzHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/MubHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/NtfsHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/PeHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/PpmdHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/QcowHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/RpmHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/SparseHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/SplitHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/SquashfsHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/SwfHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/UefiHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/VdiHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/VhdHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/VhdxHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/VmdkHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/XarHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/XzHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ZHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ZstdHandler.cpp
            # AR_COMMON_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/CoderMixer2.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/DummyOutStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/FindSignature.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/InStreamWithCRC.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/ItemNameUtils.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/MultiStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/OutStreamWithCRC.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/OutStreamWithSha1.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/HandlerOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Common/ParseProperties.cpp
            # 7Z_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zCompressionMode.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zDecode.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zEncode.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zExtract.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zFolderInStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zHandlerOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zHeader.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zIn.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zProperties.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zSpecStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zUpdate.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/7z/7zRegister.cpp
            # CAB_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Cab/CabBlockInStream.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Cab/CabHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Cab/CabHeader.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Cab/CabIn.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Cab/CabRegister.cpp
            # CHM_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Chm/ChmHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Chm/ChmIn.cpp
            # ISO_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Iso/IsoHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Iso/IsoHeader.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Iso/IsoIn.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Iso/IsoRegister.cpp
            # NSIS_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Nsis/NsisDecode.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Nsis/NsisHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Nsis/NsisIn.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Nsis/NsisRegister.cpp
            # RAR_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Rar/RarHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Rar/Rar5Handler.cpp
            # TAR_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Tar/TarHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Tar/TarHandlerOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Tar/TarHeader.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Tar/TarIn.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Tar/TarOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Tar/TarUpdate.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Tar/TarRegister.cpp
            # UDF_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Udf/UdfHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Udf/UdfIn.cpp
            # WIM_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Wim/WimHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Wim/WimHandlerOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Wim/WimIn.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Wim/WimRegister.cpp
            # ZIP_OBJS
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipAddCommon.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipHandler.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipHandlerOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipIn.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipItem.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipOut.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipUpdate.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/Zip/ZipRegister.cpp)

# Included by ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Format7zF/Arc[_gcc].mak

# ${sevenzip_SOURCE_DIR}/CPP/7zip/Aes.mak
target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/Aes.c)
if("ASM_MASM" IN_LIST enabled_languages AND NOT USE_NO_ASM)
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/AesOpt.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/AesOpt.asm PROPERTIES LANGUAGE
                                                                                     ASM_MASM)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/AesOpt.c)
endif()

# ${sevenzip_SOURCE_DIR}/CPP/7zip/Crc.mak
target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/7zCrc.c)
if("ASM_MASM" IN_LIST enabled_languages AND NOT USE_NO_ASM)
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/7zCrcOpt.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/7zCrcOpt.asm PROPERTIES LANGUAGE
                                                                                       ASM_MASM)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/7zCrcOpt.c)
endif()

# ${sevenzip_SOURCE_DIR}/CPP/7zip/Crc64.mak
target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/XzCrc64.c)
if("ASM_MASM" IN_LIST enabled_languages AND NOT USE_NO_ASM)
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/XzCrc64Opt.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/XzCrc64Opt.asm PROPERTIES LANGUAGE
                                                                                         ASM_MASM)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/XzCrc64Opt.c)
endif()

# ${sevenzip_SOURCE_DIR}/CPP/7zip/LzFindOpt.mak
if("ASM_MASM" IN_LIST enabled_languages
   AND NOT USE_NO_ASM
   AND VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/LzFindOpt.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/LzFindOpt.asm PROPERTIES LANGUAGE
                                                                                        ASM_MASM)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/LzFindOpt.c)
endif()

# ${sevenzip_SOURCE_DIR}/CPP/7zip/LzmaDec[_gcc].mak
if("ASM_MASM" IN_LIST enabled_languages
   AND NOT USE_NO_ASM
   AND VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/LzmaDecOpt.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/LzmaDecOpt.asm PROPERTIES LANGUAGE
                                                                                         ASM_MASM)
    target_compile_definitions(7zip PRIVATE Z7_LZMA_DEC_OPT)
elseif("ASM" IN_LIST enabled_languages AND NOT USE_NO_ASM)
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/arm64/LzmaDecOpt.S)
endif()

# ${sevenzip_SOURCE_DIR}/CPP/7zip/Sha1.mak
target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/CPP/Common/Sha1Prepare.cpp
                            ${sevenzip_SOURCE_DIR}/C/Sha1.c)
if("ASM_MASM" IN_LIST enabled_languages AND NOT USE_NO_ASM)
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/Sha1Opt.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/Sha1Opt.asm PROPERTIES LANGUAGE
                                                                                      ASM_MASM)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/Sha1Opt.c)
endif()

# ${sevenzip_SOURCE_DIR}/CPP/7zip/Sha256.mak
target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/CPP/Common/Sha256Prepare.cpp
                            ${sevenzip_SOURCE_DIR}/C/Sha256.c)
if("ASM_MASM" IN_LIST enabled_languages AND NOT USE_NO_ASM)
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/Sha256Opt.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/Sha256Opt.asm PROPERTIES LANGUAGE
                                                                                        ASM_MASM)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/Sha256Opt.c)
endif()

# Sort
if("ASM_MASM" IN_LIST enabled_languages AND NOT USE_NO_ASM)
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/Asm/x86/Sort.asm)
    set_source_files_properties(${sevenzip_SOURCE_DIR}/Asm/x86/Sort.asm PROPERTIES LANGUAGE
                                                                                   ASM_MASM)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/Sort.c)
endif()

# ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Format7zF/makefile[_gcc].mak
# ${sevenzip_SOURCE_DIR}/CPP/7zip/7zip[_gcc].mak

target_compile_definitions(7zip PRIVATE Z7_EXTERNAL_CODECS Z7_PPMD_SUPPORT)
target_sources(
    7zip
    PRIVATE ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/ArchiveExports.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Archive/DllExports2.cpp
            ${sevenzip_SOURCE_DIR}/CPP/7zip/Compress/CodecExports.cpp)
if(WIN32)
    target_compile_definitions(7zip PUBLIC -DUNICODE -D_UNICODE)
    target_sources(7zip PRIVATE ${CMAKE_CURRENT_LIST_DIR}/sevenzip.def
                                ${sevenzip_SOURCE_DIR}/CPP/7zip/Bundles/Format7zF/resource.rc)
else()
    target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/CPP/Common/MyWindows.cpp)
endif()
if(NOT MSVC)
    target_compile_definitions(7zip PUBLIC -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)
endif()

# LZMA SDK API, cf. #44085. Implementation is already in the 7zip lib.

target_sources(7zip PRIVATE ${sevenzip_SOURCE_DIR}/C/LzmaLib.c)
