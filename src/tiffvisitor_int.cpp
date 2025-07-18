// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

// included header files
#include "tiffvisitor_int.hpp"  // see bug #487

#include "config.h"
#include "enforce.hpp"
#include "exif.hpp"
#include "image_int.hpp"
#include "iptc.hpp"
#include "makernote_int.hpp"
#include "photoshop.hpp"
#include "safe_op.hpp"
#include "sonymn_int.hpp"
#include "tags_int.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffimage_int.hpp"
#include "value.hpp"
#include "xmp_exiv2.hpp"

#include <functional>
#include <iostream>

// *****************************************************************************
namespace {
//! Unary predicate that matches an Exifdatum with a given group and index.
class FindExifdatum2 {
 public:
  //! Constructor, initializes the object with the group and index to look for.
  FindExifdatum2(Exiv2::IfdId group, int idx) : groupName_(Exiv2::Internal::groupName(group)), idx_(idx) {
  }
  //! Returns true if group and index match.
  bool operator()(const Exiv2::Exifdatum& md) const {
    return idx_ == md.idx() && md.groupName() == groupName_;
  }

 private:
  const char* groupName_;
  int idx_;

};  // class FindExifdatum2

Exiv2::ByteOrder stringToByteOrder(std::string_view val) {
  if (val == "II")
    return Exiv2::littleEndian;
  if (val == "MM")
    return Exiv2::bigEndian;

  return Exiv2::invalidByteOrder;
}
}  // namespace

// *****************************************************************************
// class member definitions
namespace Exiv2::Internal {
void TiffVisitor::setGo(GoEvent event, bool go) {
  go_[event] = go;
}

bool TiffVisitor::go(GoEvent event) const {
  return go_[event];
}

void TiffVisitor::visitDirectoryNext(TiffDirectory* /*object*/) {
}

void TiffVisitor::visitDirectoryEnd(TiffDirectory* /*object*/) {
}

void TiffVisitor::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/) {
}

void TiffVisitor::visitBinaryArrayEnd(TiffBinaryArray* /*object*/) {
}

void TiffFinder::init(uint16_t tag, IfdId group) {
  tag_ = tag;
  group_ = group;
  tiffComponent_ = nullptr;
  setGo(geTraverse, true);
}

void TiffFinder::findObject(TiffComponent* object) {
  if (object->tag() == tag_ && object->group() == group_) {
    tiffComponent_ = object;
    setGo(geTraverse, false);
  }
}

void TiffFinder::visitEntry(TiffEntry* object) {
  findObject(object);
}

void TiffFinder::visitDataEntry(TiffDataEntry* object) {
  findObject(object);
}

void TiffFinder::visitImageEntry(TiffImageEntry* object) {
  findObject(object);
}

void TiffFinder::visitSizeEntry(TiffSizeEntry* object) {
  findObject(object);
}

void TiffFinder::visitDirectory(TiffDirectory* object) {
  findObject(object);
}

void TiffFinder::visitSubIfd(TiffSubIfd* object) {
  findObject(object);
}

void TiffFinder::visitMnEntry(TiffMnEntry* object) {
  findObject(object);
}

void TiffFinder::visitIfdMakernote(TiffIfdMakernote* object) {
  findObject(object);
}

void TiffFinder::visitBinaryArray(TiffBinaryArray* object) {
  findObject(object);
}

void TiffFinder::visitBinaryElement(TiffBinaryElement* object) {
  findObject(object);
}

TiffCopier::TiffCopier(TiffComponent* pRoot, uint32_t root, const TiffHeaderBase* pHeader,
                       PrimaryGroups pPrimaryGroups) :
    pRoot_(pRoot), root_(root), pHeader_(pHeader), pPrimaryGroups_(std::move(pPrimaryGroups)) {
}

void TiffCopier::copyObject(const TiffComponent* object) {
  if (pHeader_->isImageTag(object->tag(), object->group(), pPrimaryGroups_)) {
    auto clone = object->clone();
    // Assumption is that the corresponding TIFF entry doesn't exist
    auto tiffPath = TiffCreator::getPath(object->tag(), object->group(), root_);
    pRoot_->addPath(object->tag(), tiffPath, pRoot_, std::move(clone));
#ifdef EXIV2_DEBUG_MESSAGES
    ExifKey key(object->tag(), groupName(object->group()));
    std::cerr << "Copied " << key << "\n";
#endif
  }
}

void TiffCopier::visitEntry(TiffEntry* object) {
  copyObject(object);
}

void TiffCopier::visitDataEntry(TiffDataEntry* object) {
  copyObject(object);
}

void TiffCopier::visitImageEntry(TiffImageEntry* object) {
  copyObject(object);
}

void TiffCopier::visitSizeEntry(TiffSizeEntry* object) {
  copyObject(object);
}

void TiffCopier::visitDirectory(TiffDirectory* /*object*/) {
  // Do not copy directories (avoids problems with SubIfds)
}

void TiffCopier::visitSubIfd(TiffSubIfd* object) {
  copyObject(object);
}

void TiffCopier::visitMnEntry(TiffMnEntry* object) {
  copyObject(object);
}

void TiffCopier::visitIfdMakernote(TiffIfdMakernote* object) {
  copyObject(object);
}

void TiffCopier::visitBinaryArray(TiffBinaryArray* object) {
  copyObject(object);
}

void TiffCopier::visitBinaryElement(TiffBinaryElement* object) {
  copyObject(object);
}

TiffDecoder::TiffDecoder(ExifData& exifData, IptcData& iptcData, XmpData& xmpData, TiffComponent* pRoot,
                         FindDecoderFct findDecoderFct) :
    exifData_(exifData), iptcData_(iptcData), xmpData_(xmpData), pRoot_(pRoot), findDecoderFct_(findDecoderFct) {
  // #1402 Fujifilm RAF. Search for the make
  // Find camera make in existing metadata (read from the JPEG)
  ExifKey key("Exif.Image.Make");
  if (exifData_.findKey(key) != exifData_.end()) {
    make_ = exifData_.findKey(key)->toString();
  } else {
    // Find camera make by looking for tag 0x010f in IFD0
    TiffFinder finder(0x010f, IfdId::ifd0Id);
    pRoot_->accept(finder);
    auto te = dynamic_cast<const TiffEntryBase*>(finder.result());
    if (te && te->pValue()) {
      make_ = te->pValue()->toString();
    }
  }
}

void TiffDecoder::visitEntry(TiffEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitDataEntry(TiffDataEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitImageEntry(TiffImageEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitSizeEntry(TiffSizeEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitDirectory(TiffDirectory* /* object */) {
  // Nothing to do
}

void TiffDecoder::visitSubIfd(TiffSubIfd* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitMnEntry(TiffMnEntry* object) {
  // Always decode binary makernote tag
  decodeTiffEntry(object);
}

void TiffDecoder::visitIfdMakernote(TiffIfdMakernote* object) {
  exifData_["Exif.MakerNote.Offset"] = static_cast<uint32_t>(object->mnOffset());
  switch (object->byteOrder()) {
    case littleEndian:
      exifData_["Exif.MakerNote.ByteOrder"] = "II";
      break;
    case bigEndian:
      exifData_["Exif.MakerNote.ByteOrder"] = "MM";
      break;
    case invalidByteOrder:
      break;
  }
}

void TiffDecoder::getObjData(const byte*& pData, size_t& size, uint16_t tag, IfdId group, const TiffEntryBase* object) {
  if (object && object->tag() == tag && object->group() == group) {
    pData = object->pData();
    size = object->size();
    return;
  }
  TiffFinder finder(tag, group);
  pRoot_->accept(finder);
  if (auto te = dynamic_cast<const TiffEntryBase*>(finder.result())) {
    pData = te->pData();
    size = te->size();
    return;
  }
}

void TiffDecoder::decodeXmp(const TiffEntryBase* object) {
  // add Exif tag anyway
  decodeStdTiffEntry(object);

  const byte* pData = nullptr;
  size_t size = 0;
  getObjData(pData, size, 0x02bc, IfdId::ifd0Id, object);
  if (pData) {
    std::string xmpPacket;
    xmpPacket.assign(reinterpret_cast<const char*>(pData), size);
    std::string::size_type idx = xmpPacket.find_first_of('<');
    if (idx != std::string::npos && idx > 0) {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Removing " << idx << " characters from the beginning of the XMP packet\n";
#endif
      xmpPacket = xmpPacket.substr(idx);
    }
    if (XmpParser::decode(xmpData_, xmpPacket)) {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Failed to decode XMP metadata.\n";
#endif
    }
  }
}  // TiffDecoder::decodeXmp

void TiffDecoder::decodeIptc(const TiffEntryBase* object) {
  // add Exif tag anyway
  decodeStdTiffEntry(object);

  // All tags are read at this point, so the first time we come here,
  // find the relevant IPTC tag and decode IPTC if found
  if (decodedIptc_) {
    return;
  }
  decodedIptc_ = true;
  // 1st choice: IPTCNAA
  const byte* pData = nullptr;
  size_t size = 0;
  getObjData(pData, size, 0x83bb, IfdId::ifd0Id, object);
  if (pData) {
    if (0 == IptcParser::decode(iptcData_, pData, size)) {
      return;
    }
#ifndef SUPPRESS_WARNINGS
    EXV_WARNING << "Failed to decode IPTC block found in " << "Directory Image, entry 0x83bb\n";

#endif
  }

  // 2nd choice if no IPTCNAA record found or failed to decode it:
  // ImageResources
  pData = nullptr;
  size = 0;
  getObjData(pData, size, 0x8649, IfdId::ifd0Id, object);
  if (pData) {
    const byte* record = nullptr;
    uint32_t sizeHdr = 0;
    uint32_t sizeData = 0;
    if (0 != Photoshop::locateIptcIrb(pData, size, &record, sizeHdr, sizeData)) {
      return;
    }
    if (0 == IptcParser::decode(iptcData_, record + sizeHdr, sizeData)) {
      return;
    }
#ifndef SUPPRESS_WARNINGS
    EXV_WARNING << "Failed to decode IPTC block found in " << "Directory Image, entry 0x8649\n";

#endif
  }
}  // TiffMetadataDecoder::decodeIptc

static const TagInfo* findTag(const TagInfo* pList, uint16_t tag) {
  while (pList->tag_ != 0xffff && pList->tag_ != tag)
    pList++;
  return pList->tag_ != 0xffff ? pList : nullptr;
}

TiffDataEntryBase::TiffDataEntryBase(uint16_t tag, IfdId group, uint16_t szTag, IfdId szGroup) :
    TiffEntryBase(tag, group), szTag_(szTag), szGroup_(szGroup) {
}

TiffDataEntryBase::~TiffDataEntryBase() = default;

void TiffDecoder::decodeCanonAFInfo(const TiffEntryBase* object) {
  // report Exif.Canon.AFInfo as usual
  TiffDecoder::decodeStdTiffEntry(object);
  if (object->pValue()->count() < 3 || object->pValue()->typeId() != unsignedShort)
    return;  // insufficient data

  // create vector of signedShorts from unsignedShorts in Exif.Canon.AFInfo
  std::vector<int16_t> ints;
  std::vector<uint16_t> uint;
  for (size_t i = 0; i < object->pValue()->count(); i++) {
    ints.push_back(object->pValue()->toInt64(i));
    uint.push_back(object->pValue()->toInt64(i));
  }
  // Check this is AFInfo2 (ints[0] = bytes in object)
  if (ints.front() != static_cast<int16_t>(object->pValue()->count()) * 2)
    return;

  std::string familyGroup(std::string("Exif.") + groupName(object->group()) + ".");

  const uint16_t nPoints = uint.at(2);
  const uint16_t nMasks = (nPoints + 15) / (sizeof(uint16_t) * 8);
  int nStart = 0;

  const std::tuple<uint16_t, uint16_t, bool> records[] = {
      {0x2600, 1, true},        // AFInfoSize
      {0x2601, 1, true},        // AFAreaMode
      {0x2602, 1, true},        // AFNumPoints
      {0x2603, 1, true},        // AFValidPoints
      {0x2604, 1, true},        // AFCanonImageWidth
      {0x2605, 1, true},        // AFCanonImageHeight
      {0x2606, 1, true},        // AFImageWidth"
      {0x2607, 1, true},        // AFImageHeight
      {0x2608, nPoints, true},  // AFAreaWidths
      {0x2609, nPoints, true},  // AFAreaHeights
      {0x260a, nPoints, true},  // AFXPositions
      {0x260b, nPoints, true},  // AFYPositions
      {0x260c, nMasks, false},  // AFPointsInFocus
      {0x260d, nMasks, false},  // AFPointsSelected
      {0x260e, nMasks, false},  // AFPointsUnusable
  };
  // check we have enough data!
  uint16_t count = 0;
  for (const auto& [tag, size, bSigned] : records) {
    count += size;
    if (count > ints.size())
      return;
  }

  for (const auto& [tag, size, bSigned] : records) {
    const TagInfo* pTags = ExifTags::tagList("Canon");
    if (auto pTag = findTag(pTags, tag)) {
      auto v = Exiv2::Value::create(bSigned ? Exiv2::signedShort : Exiv2::unsignedShort);
      std::string s;
      if (bSigned) {
        for (uint16_t k = 0; k < size; k++)
          s += stringFormat(" {}", ints.at(nStart++));
      } else {
        for (uint16_t k = 0; k < size; k++)
          s += stringFormat(" {}", uint.at(nStart++));
      }

      v->read(s);
      exifData_[familyGroup + pTag->name_] = *v;
    }
  }
}

void TiffDecoder::decodeTiffEntry(const TiffEntryBase* object) {
  // Don't decode the entry if value is not set
  if (!object->pValue())
    return;

  // skip decoding if decoderFct == 0
  if (auto decoderFct = findDecoderFct_(make_, object->tag(), object->group()))
    std::invoke(decoderFct, *this, object);
}  // TiffDecoder::decodeTiffEntry

void TiffDecoder::decodeStdTiffEntry(const TiffEntryBase* object) {
  ExifKey key(object->tag(), groupName(object->group()));
  key.setIdx(object->idx());
  exifData_.add(key, object->pValue());

}  // TiffDecoder::decodeTiffEntry

void TiffDecoder::visitBinaryArray(TiffBinaryArray* object) {
  if (!object->cfg() || !object->decoded()) {
    decodeTiffEntry(object);
  }
}

void TiffDecoder::visitBinaryElement(TiffBinaryElement* object) {
  decodeTiffEntry(object);
}

TiffEncoder::TiffEncoder(ExifData& exifData, IptcData& iptcData, XmpData& xmpData, TiffComponent* pRoot,
                         bool isNewImage, PrimaryGroups pPrimaryGroups, const TiffHeaderBase* pHeader,
                         FindEncoderFct findEncoderFct) :
    exifData_(exifData),
    iptcData_(iptcData),
    xmpData_(xmpData),
    pHeader_(pHeader),
    pRoot_(pRoot),
    isNewImage_(isNewImage),
    pPrimaryGroups_(std::move(pPrimaryGroups)),
    byteOrder_(pHeader->byteOrder()),
    origByteOrder_(byteOrder_),
    findEncoderFct_(findEncoderFct) {
  encodeIptc();
  encodeXmp();

  // Find camera make
  ExifKey key("Exif.Image.Make");
  if (auto pos = exifData_.findKey(key); pos != exifData_.end()) {
    make_ = pos->toString();
  }
  if (make_.empty() && pRoot_) {
    TiffFinder finder(0x010f, IfdId::ifd0Id);
    pRoot_->accept(finder);
    auto te = dynamic_cast<const TiffEntryBase*>(finder.result());
    if (te && te->pValue()) {
      make_ = te->pValue()->toString();
    }
  }
}

void TiffEncoder::encodeIptc() {
  // Update IPTCNAA Exif tag, if it exists. Delete the tag if there
  // is no IPTC data anymore.
  // If there is new IPTC data and Exif.Image.ImageResources does
  // not exist, create a new IPTCNAA Exif tag.
  bool del = false;
  ExifKey iptcNaaKey("Exif.Image.IPTCNAA");
  auto pos = exifData_.findKey(iptcNaaKey);
  if (pos != exifData_.end()) {
    iptcNaaKey.setIdx(pos->idx());
    exifData_.erase(pos);
    del = true;
  }
  DataBuf rawIptc = IptcParser::encode(iptcData_);
  ExifKey irbKey("Exif.Image.ImageResources");
  pos = exifData_.findKey(irbKey);
  if (pos != exifData_.end()) {
    irbKey.setIdx(pos->idx());
  }
  if (!rawIptc.empty() && (del || pos == exifData_.end())) {
    auto value = Value::create(unsignedLong);
    DataBuf buf;
    if (rawIptc.size() % 4 != 0) {
      // Pad the last unsignedLong value with 0s
      buf.alloc(((rawIptc.size() / 4) * 4) + 4);
      std::move(rawIptc.begin(), rawIptc.end(), buf.begin());
    } else {
      buf = std::move(rawIptc);  // Note: This resets rawIptc
    }
    value->read(buf.data(), buf.size(), byteOrder_);
    Exifdatum iptcDatum(iptcNaaKey, value.get());
    exifData_.add(iptcDatum);
    pos = exifData_.findKey(irbKey);  // needed after add()
  }
  // Also update IPTC IRB in Exif.Image.ImageResources if it exists,
  // but don't create it if not.
  if (pos != exifData_.end()) {
    DataBuf irbBuf(pos->value().size());
    pos->value().copy(irbBuf.data(), invalidByteOrder);
    irbBuf = Photoshop::setIptcIrb(irbBuf.c_data(), irbBuf.size(), iptcData_);
    exifData_.erase(pos);
    if (!irbBuf.empty()) {
      auto value = Value::create(unsignedByte);
      value->read(irbBuf.data(), irbBuf.size(), invalidByteOrder);
      Exifdatum iptcDatum(irbKey, value.get());
      exifData_.add(iptcDatum);
    }
  }
}  // TiffEncoder::encodeIptc

void TiffEncoder::encodeXmp() {
#ifdef EXV_HAVE_XMP_TOOLKIT
  ExifKey xmpKey("Exif.Image.XMLPacket");
  // Remove any existing XMP Exif tag
  if (auto pos = exifData_.findKey(xmpKey); pos != exifData_.end()) {
    xmpKey.setIdx(pos->idx());
    exifData_.erase(pos);
  }
  std::string xmpPacket;
  if (xmpData_.usePacket()) {
    xmpPacket = xmpData_.xmpPacket();
  } else {
    if (XmpParser::encode(xmpPacket, xmpData_) > 1) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Failed to encode XMP metadata.\n";
#endif
    }
  }
  if (!xmpPacket.empty()) {
    // Set the XMP Exif tag to the new value
    auto value = Value::create(unsignedByte);
    value->read(reinterpret_cast<const byte*>(xmpPacket.data()), xmpPacket.size(), invalidByteOrder);
    Exifdatum xmpDatum(xmpKey, value.get());
    exifData_.add(xmpDatum);
  }
#endif
}  // TiffEncoder::encodeXmp

void TiffEncoder::setDirty(bool flag) {
  dirty_ = flag;
  setGo(geTraverse, !flag);
}

bool TiffEncoder::dirty() const {
  return dirty_ || !exifData_.empty();
}

void TiffEncoder::visitEntry(TiffEntry* object) {
  encodeTiffComponent(object);
}

void TiffEncoder::visitDataEntry(TiffDataEntry* object) {
  encodeTiffComponent(object);
}

void TiffEncoder::visitImageEntry(TiffImageEntry* object) {
  encodeTiffComponent(object);
}

void TiffEncoder::visitSizeEntry(TiffSizeEntry* object) {
  encodeTiffComponent(object);
}

void TiffEncoder::visitDirectory(TiffDirectory* /*object*/) {
  // Nothing to do
}

void TiffEncoder::visitDirectoryNext(TiffDirectory* object) {
  // Update type and count in IFD entries, in case they changed
  byte* p = object->start() + 2;
  for (const auto& component : object->components_) {
    p += updateDirEntry(p, byteOrder(), component.get());
  }
}

uint32_t TiffEncoder::updateDirEntry(byte* buf, ByteOrder byteOrder, TiffComponent* pTiffComponent) {
  auto pTiffEntry = dynamic_cast<const TiffEntryBase*>(pTiffComponent);
  if (!pTiffEntry)
    return 0;
  us2Data(buf + 2, pTiffEntry->tiffType(), byteOrder);
  ul2Data(buf + 4, static_cast<uint32_t>(pTiffEntry->count()), byteOrder);
  // Move data to offset field, if it fits and is not yet there.
  if (pTiffEntry->size() <= 4 && buf + 8 != pTiffEntry->pData()) {
#ifdef EXIV2_DEBUG_MESSAGES
    std::cerr << "Copying data for tag " << pTiffEntry->tag() << " to offset area.\n";
#endif
    memset(buf + 8, 0x0, 4);
    if (pTiffEntry->size() > 0) {
      std::copy_n(pTiffEntry->pData(), pTiffEntry->size(), buf + 8);
      memset(const_cast<byte*>(pTiffEntry->pData()), 0x0, pTiffEntry->size());
    }
  }
  return 12;
}

void TiffEncoder::visitSubIfd(TiffSubIfd* object) {
  encodeTiffComponent(object);
}

void TiffEncoder::visitMnEntry(TiffMnEntry* object) {
  // Test is required here as well as in the callback encoder function
  if (!object->mn_) {
    encodeTiffComponent(object);
  } else if (del_) {
    // The makernote is made up of decoded tags, delete binary tag
    ExifKey key(object->tag(), groupName(object->group()));
    auto pos = exifData_.findKey(key);
    if (pos != exifData_.end())
      exifData_.erase(pos);
  }
}

void TiffEncoder::visitIfdMakernote(TiffIfdMakernote* object) {
  auto pos = exifData_.findKey(ExifKey("Exif.MakerNote.ByteOrder"));
  if (pos != exifData_.end()) {
    // Set Makernote byte order
    ByteOrder bo = stringToByteOrder(pos->toString());
    if (bo != invalidByteOrder && bo != object->byteOrder()) {
      object->setByteOrder(bo);
      setDirty();
    }
    if (del_)
      exifData_.erase(pos);
  }
  if (del_) {
    // Remove remaining synthesized tags
    static constexpr auto synthesizedTags = std::array{
        "Exif.MakerNote.Offset",
    };
    for (auto synthesizedTag : synthesizedTags) {
      pos = exifData_.findKey(ExifKey(synthesizedTag));
      if (pos != exifData_.end())
        exifData_.erase(pos);
    }
  }
  // Modify encoder for Makernote peculiarities, byte order
  byteOrder_ = object->byteOrder();

}  // TiffEncoder::visitIfdMakernote

void TiffEncoder::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/) {
  // Reset byte order back to that from the c'tor
  byteOrder_ = origByteOrder_;

}  // TiffEncoder::visitIfdMakernoteEnd

void TiffEncoder::visitBinaryArray(TiffBinaryArray* object) {
  if (!object->cfg() || !object->decoded()) {
    encodeTiffComponent(object);
  }
}

void TiffEncoder::visitBinaryArrayEnd(TiffBinaryArray* object) {
  if (!object->cfg() || !object->decoded())
    return;
  size_t size = object->TiffEntryBase::doSize();
  if (size == 0)
    return;
  if (!object->initialize(pRoot_))
    return;

  // Re-encrypt buffer if necessary
  CryptFct cryptFct = object->cfg()->cryptFct_;
  if (cryptFct == &sonyTagDecipher) {
    cryptFct = sonyTagEncipher;
  }
  if (cryptFct) {
    const byte* pData = object->pData();
    DataBuf buf = cryptFct(object->tag(), pData, size, pRoot_);
    if (!buf.empty()) {
      pData = buf.c_data();
      size = buf.size();
    }
    if (!object->updOrigDataBuf(pData, size)) {
      setDirty();
    }
  }
}

void TiffEncoder::visitBinaryElement(TiffBinaryElement* object) {
  // Temporarily overwrite byte order according to that of the binary element
  ByteOrder boOrig = byteOrder_;
  if (object->elByteOrder() != invalidByteOrder)
    byteOrder_ = object->elByteOrder();
  encodeTiffComponent(object);
  byteOrder_ = boOrig;
}

bool TiffEncoder::isImageTag(uint16_t tag, IfdId group) const {
  return !isNewImage_ && pHeader_->isImageTag(tag, group, pPrimaryGroups_);
}

void TiffEncoder::encodeTiffComponent(TiffEntryBase* object, const Exifdatum* datum) {
  auto pos = exifData_.end();
  const Exifdatum* ed = datum;
  if (!ed) {
    // Non-intrusive writing: find matching tag
    ExifKey key(object->tag(), groupName(object->group()));
    pos = exifData_.findKey(key);
    if (pos != exifData_.end()) {
      ed = &(*pos);
      if (object->idx() != pos->idx()) {
        // Try to find exact match (in case of duplicate tags)
        auto pos2 = std::find_if(exifData_.begin(), exifData_.end(), FindExifdatum2(object->group(), object->idx()));
        if (pos2 != exifData_.end() && pos2->key() == key.key()) {
          ed = &(*pos2);
          pos = pos2;  // make sure we delete the correct tag below
        }
      }
    } else {
      setDirty();
#ifdef EXIV2_DEBUG_MESSAGES
      std::cerr << "DELETING          " << key << ", idx = " << object->idx() << "\n";
#endif
    }
  } else {
    // For intrusive writing, the index is used to preserve the order of
    // duplicate tags
    object->idx_ = ed->idx();
  }
  // Skip encoding image tags of existing TIFF image - they were copied earlier -
  // but encode image tags of new images (creation)
  if (ed && !isImageTag(object->tag(), object->group())) {
    if (auto fct = findEncoderFct_(make_, object->tag(), object->group())) {
      // If an encoding function is registered for the tag, use it
      std::invoke(fct, *this, object, ed);
    } else {
      // Else use the encode function at the object (results in a double-dispatch
      // to the appropriate encoding function of the encoder.
      object->encode(*this, ed);
    }
  }
  if (del_ && pos != exifData_.end()) {
    exifData_.erase(pos);
  }
#ifdef EXIV2_DEBUG_MESSAGES
  std::cerr << "\n";
#endif
}  // TiffEncoder::encodeTiffComponent

void TiffEncoder::encodeBinaryArray(TiffBinaryArray* object, const Exifdatum* datum) {
  encodeOffsetEntry(object, datum);
}  // TiffEncoder::encodeBinaryArray

void TiffEncoder::encodeBinaryElement(TiffBinaryElement* object, const Exifdatum* datum) {
  encodeTiffEntryBase(object, datum);
}  // TiffEncoder::encodeArrayElement

void TiffEncoder::encodeDataEntry(TiffDataEntry* object, const Exifdatum* datum) {
  encodeOffsetEntry(object, datum);

  if (!dirty_ && writeMethod() == wmNonIntrusive) {
    if (object->sizeDataArea_ < object->pValue()->sizeDataArea()) {
#ifdef EXIV2_DEBUG_MESSAGES
      ExifKey key(object->tag(), groupName(object->group()));
      std::cerr << "DATAAREA GREW     " << key << "\n";
#endif
      setDirty();
    } else {
      // Write the new dataarea, fill with 0x0
#ifdef EXIV2_DEBUG_MESSAGES
      ExifKey key(object->tag(), groupName(object->group()));
      std::cerr << "Writing data area for " << key << "\n";
#endif
      DataBuf buf = object->pValue()->dataArea();
      if (!buf.empty()) {
        std::copy(buf.begin(), buf.end(), object->pDataArea_);
        if (object->sizeDataArea_ > buf.size()) {
          memset(object->pDataArea_ + buf.size(), 0x0, object->sizeDataArea_ - buf.size());
        }
      }
    }
  }

}  // TiffEncoder::encodeDataEntry

void TiffEncoder::encodeTiffEntry(TiffEntry* object, const Exifdatum* datum) {
  encodeTiffEntryBase(object, datum);
}  // TiffEncoder::encodeTiffEntry

void TiffEncoder::encodeImageEntry(TiffImageEntry* object, const Exifdatum* datum) {
  encodeOffsetEntry(object, datum);

  size_t sizeDataArea = object->pValue()->sizeDataArea();

  if (sizeDataArea > 0 && writeMethod() == wmNonIntrusive) {
#ifdef EXIV2_DEBUG_MESSAGES
    std::cerr << "\t DATAAREA IS SET (NON-INTRUSIVE WRITING)";
#endif
    setDirty();
  }

  if (sizeDataArea > 0 && writeMethod() == wmIntrusive) {
#ifdef EXIV2_DEBUG_MESSAGES
    std::cerr << "\t DATAAREA IS SET (INTRUSIVE WRITING)";
#endif
    // Set pseudo strips (without a data pointer) from the size tag
    ExifKey key(object->szTag(), groupName(object->szGroup()));
    auto pos = exifData_.findKey(key);
    const byte* zero = nullptr;
    if (pos == exifData_.end()) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Size tag " << key << " not found. Writing only one strip.\n";
#endif
      object->strips_.clear();
      object->strips_.emplace_back(zero, sizeDataArea);
    } else {
      size_t sizeTotal = 0;
      object->strips_.clear();
      for (size_t i = 0; i < pos->count(); ++i) {
        uint32_t len = pos->toUint32(i);
        object->strips_.emplace_back(zero, len);
        sizeTotal += len;
      }
      if (sizeTotal != sizeDataArea) {
#ifndef SUPPRESS_WARNINGS
        ExifKey key2(object->tag(), groupName(object->group()));
        EXV_ERROR << "Sum of all sizes of " << key << " != data size of " << key2 << ". "
                  << "This results in an invalid image.\n";
#endif
        // Todo: How to fix? Write only one strip?
      }
    }
  }

  if (sizeDataArea == 0 && writeMethod() == wmIntrusive) {
#ifdef EXIV2_DEBUG_MESSAGES
    std::cerr << "\t USE STRIPS FROM SOURCE TREE IMAGE ENTRY";
#endif
    // Set strips from source tree
    if (pSourceTree_) {
      TiffFinder finder(object->tag(), object->group());
      pSourceTree_->accept(finder);
      if (auto ti = dynamic_cast<const TiffImageEntry*>(finder.result())) {
        object->strips_ = ti->strips_;
      }
    }
#ifndef SUPPRESS_WARNINGS
    else {
      ExifKey key2(object->tag(), groupName(object->group()));
      EXV_WARNING << "No image data to encode " << key2 << ".\n";
    }
#endif
  }

}  // TiffEncoder::encodeImageEntry

void TiffEncoder::encodeMnEntry(TiffMnEntry* object, const Exifdatum* datum) {
  // Test is required here as well as in the visit function
  if (!object->mn_)
    encodeTiffEntryBase(object, datum);
}  // TiffEncoder::encodeMnEntry

void TiffEncoder::encodeSizeEntry(TiffSizeEntry* object, const Exifdatum* datum) {
  encodeTiffEntryBase(object, datum);
}  // TiffEncoder::encodeSizeEntry

void TiffEncoder::encodeSubIfd(TiffSubIfd* object, const Exifdatum* datum) {
  encodeOffsetEntry(object, datum);
}  // TiffEncoder::encodeSubIfd

void TiffEncoder::encodeTiffEntryBase(TiffEntryBase* object, const Exifdatum* datum) {
#ifdef EXIV2_DEBUG_MESSAGES
  bool tooLarge = false;
#endif
  if (datum->size() > object->size_) {  // value doesn't fit, encode for intrusive writing
    setDirty();
#ifdef EXIV2_DEBUG_MESSAGES
    tooLarge = true;
#endif
  }
  object->updateValue(datum->getValue(), byteOrder());  // clones the value
#ifdef EXIV2_DEBUG_MESSAGES
  ExifKey key(object->tag(), groupName(object->group()));
  std::cerr << "UPDATING DATA     " << key;
  if (tooLarge) {
    std::cerr << "\t\t\t ALLOCATED " << std::dec << object->size_ << " BYTES";
  }
#endif
}

void TiffEncoder::encodeOffsetEntry(TiffEntryBase* object, const Exifdatum* datum) {
  size_t newSize = datum->size();
  if (newSize > object->size_) {  // value doesn't fit, encode for intrusive writing
    setDirty();
    object->updateValue(datum->getValue(), byteOrder());  // clones the value
#ifdef EXIV2_DEBUG_MESSAGES
    ExifKey key(object->tag(), groupName(object->group()));
    std::cerr << "UPDATING DATA     " << key;
    std::cerr << "\t\t\t ALLOCATED " << object->size() << " BYTES";
#endif
  } else {
    object->setValue(datum->getValue());  // clones the value
#ifdef EXIV2_DEBUG_MESSAGES
    ExifKey key(object->tag(), groupName(object->group()));
    std::cerr << "NOT UPDATING      " << key;
    std::cerr << "\t\t\t PRESERVE VALUE DATA";
#endif
  }
}

void TiffEncoder::add(TiffComponent* pRootDir, TiffComponent* pSourceDir, uint32_t root) {
  writeMethod_ = wmIntrusive;
  pSourceTree_ = pSourceDir;

  // Ensure that the exifData_ entries are not deleted, to be able to
  // iterate over all remaining entries.
  del_ = false;

  auto posBo = exifData_.end();
  for (auto i = exifData_.begin(); i != exifData_.end(); ++i) {
    IfdId group = groupId(i->groupName());
    // Skip synthesized info tags
    if (group == IfdId::mnId) {
      if (i->tag() == 0x0002) {
        posBo = i;
      }
      continue;
    }

    // Skip image tags of existing TIFF image - they were copied earlier -
    // but add and encode image tags of new images (creation)
    if (isImageTag(i->tag(), group))
      continue;

    // Assumption is that the corresponding TIFF entry doesn't exist
    auto tiffPath = TiffCreator::getPath(i->tag(), group, root);
    TiffComponent* tc = pRootDir->addPath(i->tag(), tiffPath, pRootDir);
    auto object = dynamic_cast<TiffEntryBase*>(tc);
#ifdef EXIV2_DEBUG_MESSAGES
    if (!object) {
      std::cerr << "Warning: addPath() didn't add an entry for " << i->groupName() << " tag 0x" << std::setw(4)
                << std::setfill('0') << std::hex << i->tag() << "\n";
    }
#endif
    if (object) {
      encodeTiffComponent(object, &(*i));
    }
  }

  /*
    What follows is a hack. I can't think of a better way to set
    the makernote byte order (and other properties maybe) in the
    makernote header during intrusive writing. The thing is that
    visit/encodeIfdMakernote is not called in this case and there
    can't be an Exif tag which corresponds to this component.
   */
  if (posBo == exifData_.end())
    return;

  TiffFinder finder(0x927c, IfdId::exifId);
  pRootDir->accept(finder);
  if (auto te = dynamic_cast<const TiffMnEntry*>(finder.result())) {
    if (auto tim = dynamic_cast<TiffIfdMakernote*>(te->mn_.get())) {
      // Set Makernote byte order
      ByteOrder bo = stringToByteOrder(posBo->toString());
      if (bo != invalidByteOrder)
        tim->setByteOrder(bo);
    }
  }

}  // TiffEncoder::add

TiffReader::TiffReader(const byte* pData, size_t size, TiffComponent* pRoot, TiffRwState state) :
    pData_(pData), size_(size), pLast_(pData + size), pRoot_(pRoot), origState_(state), mnState_(state) {
  pState_ = &origState_;

}  // TiffReader::TiffReader

void TiffReader::setOrigState() {
  pState_ = &origState_;
}

void TiffReader::setMnState(const TiffRwState* state) {
  if (state) {
    // invalidByteOrder indicates 'no change'
    if (state->byteOrder() == invalidByteOrder) {
      mnState_ = TiffRwState{origState_.byteOrder(), state->baseOffset()};
    } else {
      mnState_ = *state;
    }
  }
  pState_ = &mnState_;
}

ByteOrder TiffReader::byteOrder() const {
  return pState_->byteOrder();
}

size_t TiffReader::baseOffset() const {
  return pState_->baseOffset();
}

void TiffReader::readDataEntryBase(TiffDataEntryBase* object) {
  readTiffEntry(object);
  TiffFinder finder(object->szTag(), object->szGroup());
  pRoot_->accept(finder);
  auto te = dynamic_cast<const TiffEntryBase*>(finder.result());
  if (te && te->pValue()) {
    object->setStrips(te->pValue(), pData_, size_, baseOffset());
  }
}

void TiffReader::visitEntry(TiffEntry* object) {
  readTiffEntry(object);
}

void TiffReader::visitDataEntry(TiffDataEntry* object) {
  readDataEntryBase(object);
}

void TiffReader::visitImageEntry(TiffImageEntry* object) {
  readDataEntryBase(object);
}

void TiffReader::visitSizeEntry(TiffSizeEntry* object) {
  readTiffEntry(object);
  TiffFinder finder(object->dtTag(), object->dtGroup());
  pRoot_->accept(finder);
  auto te = dynamic_cast<TiffDataEntryBase*>(finder.result());
  if (te && te->pValue()) {
    te->setStrips(object->pValue(), pData_, size_, baseOffset());
  }
}

bool TiffReader::circularReference(const byte* start, IfdId group) {
  if (auto pos = dirList_.find(start); pos != dirList_.end()) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << groupName(group) << " pointer references previously read " << groupName(pos->second)
              << " directory; ignored.\n";
#endif
    return true;
  }
  dirList_[start] = group;
  return false;
}

int TiffReader::nextIdx(IfdId group) {
  return ++idxSeq_[group];
}

void TiffReader::postProcess() {
  setMnState();  // All components to be post-processed must be from the Makernote
  postProc_ = true;
  for (auto pos : postList_) {
    pos->accept(*this);
  }
  postProc_ = false;
  setOrigState();
}

void TiffReader::visitDirectory(TiffDirectory* object) {
  const byte* p = object->start();

  if (circularReference(object->start(), object->group()))
    return;

  if (p + 2 > pLast_) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Directory " << groupName(object->group()) << ": IFD exceeds data buffer, cannot read entry count.\n";
#endif
    return;
  }
  const uint16_t n = getUShort(p, byteOrder());
  p += 2;
  // Sanity check with an "unreasonably" large number
  if (n > 256) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Directory " << groupName(object->group()) << " with " << n
              << " entries considered invalid; not read.\n";
#endif
    return;
  }
  for (uint16_t i = 0; i < n; ++i) {
    if (p + 12 > pLast_) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Directory " << groupName(object->group()) << ": IFD entry " << i
                << " lies outside of the data buffer.\n";
#endif
      return;
    }
    uint16_t tag = getUShort(p, byteOrder());
    if (auto tc = TiffCreator::create(tag, object->group())) {
      tc->setStart(p);
      object->addChild(std::move(tc));
    } else {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Unable to handle tag " << tag << ".\n";
#endif
    }
    p += 12;
  }

  if (object->hasNext()) {
    if (p + 4 > pLast_) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Directory " << groupName(object->group())
                << ": IFD exceeds data buffer, cannot read next pointer.\n";
#endif
      return;
    }
    TiffComponent::UniquePtr tc;
    uint32_t next = getULong(p, byteOrder());
    if (next) {
      tc = TiffCreator::create(Tag::next, object->group());
#ifndef SUPPRESS_WARNINGS
      if (!tc) {
        EXV_WARNING << "Directory " << groupName(object->group()) << " has an unexpected next pointer; ignored.\n";
      }
#endif
    }
    if (tc) {
      if (baseOffset() + next > size_) {
#ifndef SUPPRESS_WARNINGS
        EXV_ERROR << "Directory " << groupName(object->group()) << ": Next pointer is out of bounds; ignored.\n";
#endif
        return;
      }
      tc->setStart(pData_ + baseOffset() + next);
      object->addNext(std::move(tc));
    }
  }  // object->hasNext()

}  // TiffReader::visitDirectory

void TiffReader::visitSubIfd(TiffSubIfd* object) {
  readTiffEntry(object);
  if ((object->tiffType() == ttUnsignedLong || object->tiffType() == ttSignedLong || object->tiffType() == ttTiffIfd) &&
      object->count() >= 1) {
    // Todo: Fix hack
    uint32_t maxi = 9;
    if (object->group() == IfdId::ifd1Id)
      maxi = 1;
    for (uint32_t i = 0; i < object->count(); ++i) {
      uint32_t offset = getULong(object->pData() + (4 * i), byteOrder());
      if (baseOffset() + offset > size_) {
#ifndef SUPPRESS_WARNINGS
        EXV_ERROR << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                  << std::hex << object->tag() << " Sub-IFD pointer " << i << " is out of bounds; ignoring it.\n";
#endif
        return;
      }
      if (i >= maxi) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                    << std::hex << object->tag() << ": Skipping sub-IFDs beyond the first " << i << ".\n";
#endif
        break;
      }
      // If there are multiple dirs, group is incremented for each
      auto td = std::make_unique<TiffDirectory>(object->tag(),
                                                static_cast<IfdId>(static_cast<uint32_t>(object->newGroup_) + i));
      td->setStart(pData_ + baseOffset() + offset);
      object->addChild(std::move(td));
    }
  }
#ifndef SUPPRESS_WARNINGS
  else {
    EXV_WARNING << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                << std::hex << object->tag() << " doesn't look like a sub-IFD.\n";
  }
#endif

}  // TiffReader::visitSubIfd

void TiffReader::visitMnEntry(TiffMnEntry* object) {
  readTiffEntry(object);
  // Find camera make
  TiffFinder finder(0x010f, IfdId::ifd0Id);
  pRoot_->accept(finder);
  auto te = dynamic_cast<const TiffEntryBase*>(finder.result());
  if (te && te->pValue()) {
    auto make = te->pValue()->toString();
    // create concrete makernote, based on make and makernote contents
    object->mn_ =
        TiffMnCreator::create(object->tag(), object->mnGroup_, make, object->pData_, object->size_, byteOrder());
  }
  if (object->mn_)
    object->mn_->setStart(object->pData());

}  // TiffReader::visitMnEntry

void TiffReader::visitIfdMakernote(TiffIfdMakernote* object) {
  object->setImageByteOrder(byteOrder());  // set the byte order for the image

  if (!object->readHeader(object->start(), pLast_ - object->start(), byteOrder())) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Failed to read " << groupName(object->ifd_.group()) << " IFD Makernote header.\n";
#ifdef EXIV2_DEBUG_MESSAGES
    if (pLast_ - object->start() >= 16u) {
      hexdump(std::cerr, object->start(), 16u);
    }
#endif  // EXIV2_DEBUG_MESSAGES
#endif  // SUPPRESS_WARNINGS
    setGo(geKnownMakernote, false);
    return;
  }

  object->ifd_.setStart(object->start() + object->ifdOffset());

  // Modify reader for Makernote peculiarities, byte order and offset
  object->mnOffset_ = object->start() - pData_;
  auto state = TiffRwState{object->byteOrder(), object->baseOffset()};
  setMnState(&state);

}  // TiffReader::visitIfdMakernote

void TiffReader::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/) {
  // Reset state (byte order, create function, offset) back to that for the image
  setOrigState();
}  // TiffReader::visitIfdMakernoteEnd

void TiffReader::readTiffEntry(TiffEntryBase* object) {
  try {
    byte* p = object->start();

    if (p + 12 > pLast_) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Entry in directory " << groupName(object->group())
                << "requests access to memory beyond the data buffer. " << "Skipping entry.\n";
#endif
      return;
    }
    // Component already has tag
    p += 2;
    auto tiffType = static_cast<TiffType>(getUShort(p, byteOrder()));
    TypeId typeId = toTypeId(tiffType, object->tag(), object->group());
    size_t typeSize = TypeInfo::typeSize(typeId);
    if (0 == typeSize) {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                  << std::hex << object->tag() << " has unknown Exif (TIFF) type " << std::dec << tiffType
                  << "; setting type size 1.\n";
#endif
      typeSize = 1;
    }
    p += 2;
    uint32_t count = getULong(p, byteOrder());
    if (count >= 0x10000000) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                << std::hex << object->tag() << " has invalid size " << std::dec << count << "*" << typeSize
                << "; skipping entry.\n";
#endif
      return;
    }
    p += 4;

    if (count > std::numeric_limits<size_t>::max() / typeSize) {
      throw Error(ErrorCode::kerArithmeticOverflow);
    }
    size_t size = typeSize * count;
    size_t offset = getULong(p, byteOrder());
    byte* pData = p;
    if (size > 4 && Safe::add<size_t>(baseOffset(), offset) >= size_) {
      // #1143
      if (object->tag() == 0x2001 && std::string(groupName(object->group())) == "Sony1") {
        // This tag is Exif.Sony1.PreviewImage, which refers to a preview image which is
        // not stored in the metadata. Instead it is stored at the end of the file, after
        // the main image. The value of `size` refers to the size of the preview image, not
        // the size of the tag's payload, so we set it to zero here so that we don't attempt
        // to read those bytes from the metadata. We currently leave this tag as "undefined",
        // although we may attempt to handle it better in the future. More discussion of
        // this issue can be found here:
        //
        //   https://github.com/Exiv2/exiv2/issues/2001
        //   https://github.com/Exiv2/exiv2/pull/2008
        //   https://github.com/Exiv2/exiv2/pull/2013
        typeId = undefined;
        size = 0;
      } else {
#ifndef SUPPRESS_WARNINGS
        EXV_ERROR << "Offset of directory " << groupName(object->group()) << ", entry 0x" << std::setw(4)
                  << std::setfill('0') << std::hex << object->tag() << " is out of bounds: " << "Offset = 0x"
                  << std::setw(8) << std::setfill('0') << std::hex << offset << "; truncating the entry\n";
#endif
      }
      size = 0;
    }
    if (size > 4) {
      // setting pData to pData_ + baseOffset() + offset can result in pData pointing to invalid memory,
      // as offset can be arbitrarily large
      if (Safe::add<size_t>(baseOffset(), offset) > static_cast<size_t>(pLast_ - pData_)) {
        throw Error(ErrorCode::kerCorruptedMetadata);
      }
      pData = const_cast<byte*>(pData_) + baseOffset() + offset;

      // check for size being invalid
      if (size > static_cast<size_t>(pLast_ - pData)) {
#ifndef SUPPRESS_WARNINGS
        EXV_ERROR << "Upper boundary of data for " << "directory " << groupName(object->group()) << ", entry 0x"
                  << std::setw(4) << std::setfill('0') << std::hex << object->tag()
                  << " is out of bounds: " << "Offset = 0x" << std::setw(8) << std::setfill('0') << std::hex << offset
                  << ", size = " << std::dec << size
                  << ", exceeds buffer size by "
                  // cast to make MSVC happy
                  << size - static_cast<size_t>(pLast_ - pData) << " Bytes; truncating the entry\n";
#endif
        size = 0;
      }
    }
    auto v = Value::create(typeId);
    enforce(v != nullptr, ErrorCode::kerCorruptedMetadata);
    v->read(pData, size, byteOrder());

    object->setValue(std::move(v));
    auto d = std::make_shared<DataBuf>();
    object->setData(pData, size, std::move(d));
    object->setOffset(offset);
    object->setIdx(nextIdx(object->group()));
  } catch (std::overflow_error&) {
    throw Error(ErrorCode::kerCorruptedMetadata);  // #562 don't throw std::overflow_error
  }
}  // TiffReader::readTiffEntry

void TiffReader::visitBinaryArray(TiffBinaryArray* object) {
  if (!postProc_) {
    // Defer reading children until after all other components are read, but
    // since state (offset) is not set during post-processing, read entry here
    readTiffEntry(object);
    object->iniOrigDataBuf();
    postList_.push_back(object);
    return;
  }
  // Check duplicates
  TiffFinder finder(object->tag(), object->group());
  pRoot_->accept(finder);
  if (auto te = dynamic_cast<const TiffEntryBase*>(finder.result())) {
    if (te->idx() != object->idx()) {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Not decoding duplicate binary array tag 0x" << std::setw(4) << std::setfill('0') << std::hex
                  << object->tag() << std::dec << ", group " << groupName(object->group()) << ", idx " << object->idx()
                  << "\n";
#endif
      object->setDecoded(false);
      return;
    }
  }

  if (object->TiffEntryBase::doSize() == 0)
    return;
  if (!object->initialize(pRoot_))
    return;
  const ArrayCfg* cfg = object->cfg();
  if (!cfg)
    return;

  if (auto cryptFct = cfg->cryptFct_) {
    const byte* pData = object->pData();
    size_t size = object->TiffEntryBase::doSize();
    auto buf = std::make_shared<DataBuf>(cryptFct(object->tag(), pData, size, pRoot_));
    if (!buf->empty())
      object->setData(std::move(buf));
  }

  const ArrayDef* defs = object->def();
  const ArrayDef* defsEnd = defs + object->defSize();
  const ArrayDef* def = &cfg->elDefaultDef_;
  ArrayDef gap = *def;

  for (size_t idx = 0; idx < object->TiffEntryBase::doSize();) {
    if (defs) {
      def = std::find(defs, defsEnd, idx);
      if (def == defsEnd) {
        if (cfg->concat_) {
          // Determine gap-size
          const ArrayDef* xdef = defs;
          for (; xdef != defsEnd && xdef->idx_ <= idx; ++xdef) {
          }
          size_t gapSize = 0;
          if (xdef != defsEnd && xdef->idx_ > idx) {
            gapSize = xdef->idx_ - idx;
          } else {
            gapSize = object->TiffEntryBase::doSize() - idx;
          }
          gap.idx_ = idx;
          gap.tiffType_ = cfg->elDefaultDef_.tiffType_;
          gap.count_ = gapSize / cfg->tagStep();
          if (gap.count_ * cfg->tagStep() != gapSize) {
            gap.tiffType_ = ttUndefined;
            gap.count_ = gapSize;
          }
          def = &gap;
        } else {
          def = &cfg->elDefaultDef_;
        }
      }
    }
    idx += object->addElement(idx, *def);  // idx may be different from def->idx_
  }

}  // TiffReader::visitBinaryArray

void TiffReader::visitBinaryElement(TiffBinaryElement* object) {
  auto pData = object->start();
  size_t size = object->TiffEntryBase::doSize();
  ByteOrder bo = object->elByteOrder();
  if (bo == invalidByteOrder)
    bo = byteOrder();
  TypeId typeId = toTypeId(object->elDef()->tiffType_, object->tag(), object->group());
  auto v = Value::create(typeId);
  enforce(v != nullptr, ErrorCode::kerCorruptedMetadata);
  v->read(pData, size, bo);

  object->setValue(std::move(v));
  object->setOffset(0);
  object->setIdx(nextIdx(object->group()));
}

}  // namespace Exiv2::Internal
