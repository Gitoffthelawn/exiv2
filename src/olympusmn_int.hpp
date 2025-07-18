// SPDX-License-Identifier: GPL-2.0-or-later
/*!
  @file    olympusmn_int.hpp
  @brief   Olympus makernote tags.<br>References:<br>
           [1] <a href="http://park2.wakwak.com/%7Etsuruzoh/Computer/Digicams/exif-e.html#APP1">Exif file format,
  Appendix 1: MakerNote of Olympus Digicams</a> by TsuruZoh Tachibanaya<br> [2] <a
  href="http://www.sno.phy.queensu.ca/~phil/exiftool/">ExifTool</a> by Phil Harvey<br> [3] <a
  href="http://www.ozhiker.com/electronics/pjmt/jpeg_info/olympus_mn.html">Olympus Makernote Format Specification</a> by
  Evan Hunter<br> [4] email communication with <a href="mailto:wstokes@gmail.com">Will Stokes</a>
 */
#ifndef OLYMPUSMN_INT_HPP_
#define OLYMPUSMN_INT_HPP_

// *****************************************************************************
// standard includes
#include <iosfwd>

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
class ExifData;
class Value;
struct TagInfo;

namespace Internal {
// *****************************************************************************
// class definitions

//! MakerNote for Olympus cameras
class OlympusMakerNote {
 public:
  //! Return read-only list of built-in Olympus tags
  static const TagInfo* tagList();
  //! Return read-only list of built-in Olympus Camera Settings tags
  static const TagInfo* tagListCs();
  //! Return read-only list of built-in Olympus Equipment tags
  static const TagInfo* tagListEq();
  //! Return read-only list of built-in Olympus Raw Development tags
  static const TagInfo* tagListRd();
  //! Return read-only list of built-in Olympus Raw Development 2 tags
  static const TagInfo* tagListRd2();
  //! Return read-only list of built-in Olympus Image Processing tags
  static const TagInfo* tagListIp();
  //! Return read-only list of built-in Olympus Focus Info tags
  static const TagInfo* tagListFi();
  //! Return read-only list of built-in Olympus FE tags
  static const TagInfo* tagListFe();
  //! Return read-only list of built-in Olympus Raw Info tags
  static const TagInfo* tagListRi();

  //! @name Print functions for Olympus %MakerNote tags
  //@{
  //! Print 'Special Mode'
  static std::ostream& print0x0200(std::ostream& os, const Value& value, const ExifData*);
  //! Print Digital Zoom Factor
  static std::ostream& print0x0204(std::ostream& os, const Value& value, const ExifData*);
  //! Print White Balance Mode
  static std::ostream& print0x1015(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus equipment Lens type
  static std::ostream& print0x0201(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus CameraID
  static std::ostream& print0x0209(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus equipment Extender
  static std::ostream& printEq0x0301(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus camera settings Focus Mode
  static std::ostream& printCs0x0301(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus camera settings Gradation
  static std::ostream& print0x050f(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus camera settings Noise Filter
  static std::ostream& print0x0527(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus ArtFilter
  static std::ostream& print0x0529(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus focus info ManualFlash
  static std::ostream& print0x1209(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus focus distance
  static std::ostream& print0x0305(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus focus info AF Point
  static std::ostream& print0x0308(std::ostream& os, const Value& value, const ExifData*);
  //! Print Olympus generic
  static std::ostream& printGeneric(std::ostream& os, const Value& value, const ExifData*);
  //@}

 private:
  //! Tag information
  static const TagInfo tagInfo_[];
  static const TagInfo tagInfoCs_[];
  static const TagInfo tagInfoEq_[];
  static const TagInfo tagInfoRd_[];
  static const TagInfo tagInfoRd2_[];
  static const TagInfo tagInfoIp_[];
  static const TagInfo tagInfoFi_[];
  static const TagInfo tagInfoFe_[];
  static const TagInfo tagInfoRi_[];

};  // class OlympusMakerNote

}  // namespace Internal
}  // namespace Exiv2

#endif  // #ifndef OLYMPUSMN_INT_HPP_
