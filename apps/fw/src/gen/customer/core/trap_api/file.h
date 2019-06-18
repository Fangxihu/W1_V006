#ifndef __FILE_H__
#define __FILE_H__
#include <csrtypes.h>
#include <app/file/file_if.h>

/*! file  @brief Access to the read-only file-system */
/*! @sa \#StreamFileSource, \#KalimbaLoad */

#if TRAPSET_FILE

/**
 *  \brief Find the index for a named file or directory, relative to the starting
 *  directory.
 *   
 *   Leading and trailing directory separators in @a name are ignored.
 *   @a start is commonly \#FILE_ROOT.
 *   @note
 *   For file in read-write filesystem start parameter needs to be \#FILE_ROOT
 *   and path should have "/rwfs/" prepended to the file name.
 *  \param start index of directory in which to start the search 
 *  \param name the name (possibly including a path) of the item to find 
 *  \param length the number of characters in @a name
 *  \return The index of the item searched for, or \#FILE_NONE if no such item exists.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_file
 */
FILE_INDEX FileFind(FILE_INDEX start, const char * name, uint16 length);

/**
 *  \brief Find the type of a file specified by its index
 *   
 *  \param index the index of the file whose type is required
 *  \return The type of the specified file.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_file
 */
FILE_TYPE FileType(FILE_INDEX index);

/**
 *  \brief Find the index of the directory containing this file or directory. 
 *   
 *  \param item The index of the item we know about.
 *  \return The index of the directory containing item, or \#FILE_NONE.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_file
 */
FILE_INDEX FileParent(FILE_INDEX item);

/**
 *  \brief Create file in read-write filesystem.
 *   @note  
 *   read-write filesystem does not support directory structure. File path 
 *   should not contain any extra directory apart from "/rwfs/" which  
 *   needs to be prepended to the file name.
 *   
 *  \param name the name (including a path) of the item to create 
 *  \param length the number of characters in @a name
 *  \return The index of the file created, or \#FILE_NONE if could not.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_file
 */
FILE_INDEX FileCreate(const char * name, uint16 length);

/**
 *  \brief Delete file in read-write filesystem.
 *   
 *  \param index the file which needs to be deleted.
 *  \return Success or not.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_file
 */
bool FileDelete(FILE_INDEX index);

/**
 *  \brief Rename file in read-write filesystem.
 *   @note 
 *   If a file is renamed to itself then it will return FALSE.
 *   @note  
 *   For files in read-write filesystem, path needs to have "/rwfs/" prepended to
 *  the file names.
 *   
 *  \param old_path the name (including a path) of the file to rename 
 *  \param old_path_len the number of characters in old path name
 *  \param new_path the new name (including a path) of the file 
 *  \param new_path_len the number of characters in new path name
 *  \return Success or not.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_file
 */
bool FileRename(const char * old_path, uint16 old_path_len, const char * new_path, uint16 new_path_len);

/**
 *  \brief Unmount filesystem
 *   Used to free up memory on the system processor when the current file
 *  operations 
 *   have been completed. After this call the file system will be mounted again 
 *   automatically when any trap referencing it is used (such as FileFind or
 *  FileCreate).
 *   
 *   @note
 *   This trap does not support all filesystems. If the given filesystem is not
 *  supported then 
 *   this trap will return FALSE.
 *   
 *   @note
 *   For read-write filesystem mount path needs to be "/rwfs/".
 *   
 *  \param mount_path the mount path of the filesystem to unmount 
 *  \return TRUE if unmounted successfully, otherwise FALSE
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_file
 */
bool FileSystemUnmount(const char * mount_path);
#endif /* TRAPSET_FILE */
#endif
