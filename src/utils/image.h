// Copyright (c) 2011, Alexey Ivanov

#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <FreeImagePlus.h>
#include <vector>

namespace ImageUtils
{

/*!
   \brief Image class that loaded from HBITMAP and can be saved to std::vector<BYTE> of to file.
   Delayed DLL load technique is used.
*/
class AIMPCoverImage : public fipWinImage
{
public:
    enum IMAGEFORMAT { PNG_IMAGE = 0, JPEG_IMAGE, BMP_IMAGE, IMAGE_FORMATS_COUNT };

    //! Creates image from HBITMAP. Bitmap handle will NOT be released.
    explicit AIMPCoverImage(HBITMAP cover_bitmap_handle); // throws std::runtime_error

    /*!
        \brief Saves image to std::vector<BYTE> container.
        \param image_format ID of required image format. See IMAGEFORMAT for supported formats.
        \param image_data reference to vector for saving.
        \throw std::runtime_error if saving fails.
    */
    void saveToVector(IMAGEFORMAT image_format, std::vector<BYTE>& image_data) const; // throws std::runtime_error

    /*!
        \brief Saves image to file.
        \param image_format ID of required image format. See IMAGEFORMAT for supported formats.
        \param file_name file name.
        \throw std::runtime_error if saving fails.
    */
    void saveToFile(IMAGEFORMAT image_format, const std::wstring& file_name) const; // throws std::runtime_error

private:

    HBITMAP cover_bitmap_handle_;
};

/*!
    \return size of bitmap by handle.
    \throw std::runtime_error if error occured while determine size.
*/
SIZE getBitmapSize(HBITMAP bitmap_handle); // throw std::runtime_error

} // namespace ImageUtils

#endif // #ifndef IMAGE_UTILS_H
