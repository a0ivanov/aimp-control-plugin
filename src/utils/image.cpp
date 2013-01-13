// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "image.h"
#include <boost/noncopyable.hpp>
#include "utils/util.h"

namespace ImageUtils
{
namespace FreeImage
{

/* Adapter for std::vector<BYTE> to save/load into it by FreeImage library. */
class FIIOVectorHandle : boost::noncopyable
{
public:
    typedef std::vector<BYTE> ImageDataType;

    FIIOVectorHandle(ImageDataType& image_data)
        :
        cur_position_(0), image_data_(image_data)
    {}

    /* Allow implicit cast to fi_handle. */
    operator fi_handle()
    {
        return this;
    }

    static unsigned DLL_CALLCONV ReadProc(void* buffer, unsigned size, unsigned count, fi_handle handle)
    {
        FIIOVectorHandle* handle_ = static_cast<FIIOVectorHandle*>(handle);
        unsigned x;
        for (x = 0; x < count; ++x) {
            //if there isnt size bytes left to read, set pos to eof and return a short count
            if( handle_->image_data_.size() - handle_->cur_position_ < size ) {
                handle_->cur_position_ = handle_->image_data_.size();
                break;
            }
            //copy size bytes count times
            memcpy(buffer, &(handle_->image_data_[0]) + handle_->cur_position_, size);
            handle_->cur_position_ += size;
            buffer = static_cast<char*>(buffer) + size;
        }
        return x;
    }

    static unsigned DLL_CALLCONV WriteProc(void* buffer, unsigned size, unsigned count, fi_handle handle)
    {
        typedef FIIOVectorHandle::ImageDataType::size_type SizeType;
        FIIOVectorHandle* handle_ = static_cast<FIIOVectorHandle*>(handle);

        //double the data block size if we need to
        SizeType required_image_data_size = handle_->cur_position_ + (size * count);
        if ( required_image_data_size >= handle_->image_data_.capacity() ) {

            SizeType current_image_data_size = handle_->image_data_.size(); // cash current image data size
            SizeType new_capacity;
            //if we are at or above 1G, we cant double without going negative
            if (current_image_data_size & 0x40000000) {
                //max 2G
                if (current_image_data_size == 0x7FFFFFFF) {
                    return 0;
                }
                new_capacity = 0x7FFFFFFF;
            } else if (current_image_data_size == 0) {
                //default to 4K if nothing yet
                new_capacity = 4096;
            } else {
                //double size
                new_capacity = required_image_data_size << 1;
            }

            handle_->image_data_.reserve(new_capacity);
        }

        handle_->image_data_.insert( handle_->image_data_.end(),
                                     static_cast<BYTE*>(buffer),
                                     static_cast<BYTE*>(buffer) + size * count);
        handle_->cur_position_ += (size * count);
        return count;
    }

    static int DLL_CALLCONV SeekProc(fi_handle handle, long offset, int origin)
    {
        FIIOVectorHandle* handle_ = static_cast<FIIOVectorHandle*>(handle);
        switch (origin) // 0 to filelen-1 are 'inside' the file
        {
        default:
        case SEEK_SET: // can fseek() to 0-7FFFFFFF always
            if (offset >= 0) {
                handle_->cur_position_ = offset;
                return 0;
            }
            break;

        case SEEK_CUR:
        {
            INT64 new_position = static_cast<INT64>(handle_->cur_position_) + offset;
            if ( 0 <= new_position && new_position <= handle_->image_data_.size() ) {
                handle_->cur_position_ += offset;
                return 0;
            }
            break;
        }

        case SEEK_END:
            if (offset <= 0) {
                INT64 new_position = static_cast<INT64>(handle_->image_data_.size()) + offset;
                if (new_position >= 0) {
                    handle_->cur_position_ = static_cast<FIIOVectorHandle::ImageDataType::size_type>(new_position);
                    return 0;
                }
            }
            break;
        }

        return -1;
    }

    static long DLL_CALLCONV TellProc(fi_handle handle)
    {
        return static_cast<FIIOVectorHandle*>(handle)->cur_position_;
    }

    /* Initialize FreeImageIO struct. */
    void InitFreeImageIO(FreeImageIO* io)
    {
        io->read_proc  = FIIOVectorHandle::ReadProc;
        io->seek_proc  = FIIOVectorHandle::SeekProc;
        io->tell_proc  = FIIOVectorHandle::TellProc;
        io->write_proc = FIIOVectorHandle::WriteProc;
    }

private:

    ImageDataType::size_type cur_position_;
    ImageDataType& image_data_;
};

//// use for store data about error in FreeImage library.
//std::string free_library_last_error_message;
//
///* Function used to get error description from FreeImage library, see FreeImage_SetOutputMessage description. */
//void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
//    std::ostringstream msg;
//    if (fif != FIF_UNKNOWN) {
//        msg << FreeImage_GetFormatFromFIF(fif) << " format. ";
//    }
//    msg << message;
//    free_library_last_error_message = msg.str();
//}

} // namespace FreeImage


//! Get size of bitmap by it's handle.
SIZE getBitmapSize(HBITMAP bitmap_handle)
{
    BITMAP bitmap;
    int errorcode = GetObject(bitmap_handle, sizeof(bitmap), &bitmap);
    if (0 == errorcode) {
        throw std::runtime_error("Failed to get size of bitmap.");
    }

    SIZE bitmap_size = { bitmap.bmWidth, bitmap.bmHeight };
    return bitmap_size;
}

//! general tempate for convinient casting. Provide specialization for your own types.
template<typename To, typename From> To cast(From);

/*!
    \brief Converts image format ID from AIMPCoverImage to FREE_IMAGE_FORMAT.
    If AIMPCoverImage format is unknown returns FIF_UNKNOWN format.
*/
template<>
FREE_IMAGE_FORMAT cast<FREE_IMAGE_FORMAT, AIMPCoverImage::IMAGEFORMAT>(AIMPCoverImage::IMAGEFORMAT image_format)
{
    FREE_IMAGE_FORMAT freeimage_format = FIF_UNKNOWN;
    switch (image_format)
    {
    case AIMPCoverImage::PNG_IMAGE:
        freeimage_format = FIF_PNG;
        break;
    case AIMPCoverImage::BMP_IMAGE:
        freeimage_format = FIF_BMP;
        break;
    case AIMPCoverImage::JPEG_IMAGE:
        freeimage_format = FIF_JPEG;
        break;
    }
    return freeimage_format;
}

AIMPCoverImage::AIMPCoverImage(HBITMAP cover_bitmap_handle) // throws std::runtime_error
    :
    cover_bitmap_handle_(cover_bitmap_handle)
{
    BOOL result = copyFromBitmap(cover_bitmap_handle);
    if (FALSE == result) {
        throw std::runtime_error("Error occured while create AIMPCoverImage from HBITMAP.");
    }
}

void AIMPCoverImage::saveToVector(AIMPCoverImage::IMAGEFORMAT image_format, std::vector<BYTE>& image_data) const // throws std::runtime_error
{
    using namespace FreeImage;

    FIIOVectorHandle memory_handle(image_data);
    FreeImageIO io;
    memory_handle.InitFreeImageIO(&io);

    //FreeImage_SetOutputMessage(FreeImage::FreeImageErrorHandler); // set handler that fill FreeImage::free_library_last_error_message string in case of error.
    BOOL result = saveToHandle(cast<FREE_IMAGE_FORMAT>(image_format), &io, &memory_handle);
    //FreeImage_SetOutputMessage(nullptr); // remove error handler.
    if (FALSE == result) {
        using namespace Utilities;
        throw std::runtime_error(MakeString() << "Error occured while saving image in vector. Reason: " << "unknown"); // << FreeImage::free_library_last_error_message;
    }
}

void AIMPCoverImage::saveToFile(AIMPCoverImage::IMAGEFORMAT /*image_format*/, const std::wstring& file_name) const // throws std::runtime_error
{
    //FreeImage_SetOutputMessage(FreeImage::FreeImageErrorHandler); // set handler that fill FreeImage::free_library_last_error_message string in case of error.
    BOOL result = saveU(file_name.c_str(), 0); // 0 means default saving settings.
    //FreeImage_SetOutputMessage(nullptr); // remove error handler.

    if (FALSE == result) {
        using namespace Utilities;
        throw std::runtime_error(MakeString() << "Error occured while saving image to file. Reason: " << "unknown"); // << FreeImage::free_library_last_error_message;
    }
}

} // namespace ImageUtils
