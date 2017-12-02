/*
 * File-related system call implementations.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>

/*
 * open() - get the path with copyinstr, then use openfile_open and
 * filetable_place to do the real work.
 */
int
sys_open(const_userptr_t upath, int flags, mode_t mode, int *retval)
{
	const int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;

	if(flags == allflags)//check for invalid flags
		return EINVAL;

	char *kpath = (char*)kmalloc(sizeof(char)*PATH_MAX);
	struct openfile *file;
	int result = 0;
	
	//copy in the supplied pathname
	result = copyinstr(upath, kpath, PATH_MAX, NULL);
	if(result != 0) 
		return EFAULT;

	//open the file
	result = openfile_open(kpath, flags, mode, &file);
	if(result != 0)
		return EFAULT;

	//placing the file into curproc's file table
	result = filetable_place(curproc->p_filetable, file, retval);
	if(result != 0)
		return EMFILE;

	kfree(kpath);

	/* 
	 * Your implementation of system call open starts here.  
	 *
	 * Check the design document design/filesyscall.txt for the steps
	 */

	return result;
}

/*
 * read() - read data from a file
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
	int result = 0;
	struct openfile *file;
	struct iovec tempIovec;
	struct uio tempUio;

	//translate fd to open file object
	result = filetable_get(curproc->p_filetable, fd, &file);
	
	if(result != 0)
		return result;
	
	lock_acquire(file->of_offsetlock);

	//check for files opened write-only
	if(file->of_accmode == O_WRONLY)
	{	//release lock because can't read a write only file
		lock_release(file->of_offsetlock);
		return EACCES;
	}
	
	uio_kinit(&tempIovec, &tempUio, buf, size, file->of_offset, UIO_READ);
	result = VOP_READ(file->of_vnode, &tempUio);
	if(result != 0)
		return result;

	//update seek position
	file->of_offset = tempUio.uio_offset;	
	lock_release(file->of_offsetlock);
	filetable_put(curproc->p_filetable, fd, file);
	*retval = size - tempUio.uio_resid;

	result = 0;
	
	
       /* 
        * Your implementation of system call read starts here.  
        *
        * Check the design document design/filesyscall.txt for the steps
        */

       return result;
}

int sys_write(int fd, userptr_t buf, size_t size, int *retval)
{
	int result = 0;
	struct openfile *file;
	struct iovec tempIovec;
	struct uio tempUio;
	
	//translate fd to open file object
	result = filetable_get(curproc->p_filetable, fd, &file);
	
	if(result != 0)
		return result;
	
	lock_acquire(file->of_offsetlock);
	//check for files opened read-only
	if(file->of_accmode == O_RDONLY)
	{	//release lock because can't write a read only file
		lock_release(file->of_offsetlock);
		return EBADF;
	}
	
	uio_kinit(&tempIovec, &tempUio, buf, size, file->of_offset, UIO_WRITE);
	result = VOP_WRITE(file->of_vnode, &tempUio);
	if(result != 0)
		return result;

	//update seek position
	file->of_offset = tempUio.uio_offset;	
	lock_release(file->of_offsetlock);
	filetable_put(curproc->p_filetable, fd, file);
	*retval = size - tempUio.uio_resid;
	//(void)retval;
	
	return result;
}
/*
 * write() - write data to a file
 */

int sys_close(int fd)
{
	struct openfile *file;
	//validate the fd number	
	KASSERT(filetable_okfd(curproc->p_filetable, fd));
	
	filetable_placeat(curproc->p_filetable, NULL, fd, &file);
	
	if(file == NULL)
		return ENOENT;
	else //decref the open file
		openfile_decref(file);

	return 0;
	
}
/*
 * close() - remove from the file table.
 */

int sys_meld(const_userptr_t pn1, const_userptr_t pn2, const_userptr_t pn3, int *retval)
{
	struct openfile *file1, *file2, *file3;
	struct iovec tempIovec;
	struct uio tempUio1, tempUio2, writeUio;
	struct stat status;

	int result = 0;
	int count = 0;
	int fd, value;
	char *kpathPn1 = (char*)kmalloc(sizeof(char)*PATH_MAX);
	char *kpathPn2 = (char*)kmalloc(sizeof(char)*PATH_MAX);
	char *kpathPn3 = (char*)kmalloc(sizeof(char)*PATH_MAX);
	char *buf1 = (char*)kmalloc(sizeof(char)*4);
	char *buf2 = (char*)kmalloc(sizeof(char)*4);

	//copy in supplied pathnames
	result = copyinstr(pn1, kpathPn1, PATH_MAX, NULL);
	if(result != 0)
		return result;
	result = copyinstr(pn2, kpathPn2, PATH_MAX, NULL);
	if(result != 0)
		return result;
	result = copyinstr(pn3, kpathPn3, PATH_MAX, NULL);
	if(result != 0)
		return result;

	//open the first two files for reading

	result = openfile_open(kpathPn1, O_RDWR, 0664, &file1);
	if(result != 0)
		return ENOENT;
	result = openfile_open(kpathPn2, O_RDWR, 0664, &file2);
	if(result != 0)
		return ENOENT;

	//open the third file for writing
	result = openfile_open(kpathPn3, O_WRONLY | O_CREAT | O_EXCL, 0664, &file3);
	if(result != 0)
		return EEXIST;
	
	//place them in filetable 
	result = filetable_place(curproc->p_filetable, file1, &value);
	if(result != 0)
		return result;
	result = filetable_place(curproc->p_filetable, file2, &value);
	if(result != 0)
		return result;
	result = filetable_place(curproc->p_filetable, file3, &value);
	if(result != 0)
		return result;
	fd = value;
	
	result = VOP_STAT(file1->of_vnode, &status);
	int statSize = status.st_size;
	
	result = VOP_STAT(file2->of_vnode, &status);
	statSize += status.st_size;

	while(count < statSize/2){
		//file 1 reading 4 bytes
		lock_acquire(file1->of_offsetlock);
		
		uio_kinit(&tempIovec, &tempUio1, buf1, 4, file1->of_offset, UIO_READ);
		result = VOP_READ(file1->of_vnode, &tempUio1);
		if(result != 0)
			return result;
		file1->of_offset = tempUio1.uio_offset;

		lock_release(file1->of_offsetlock);
		

		//file 2 reading 4 bytes
		lock_acquire(file2->of_offsetlock);
		uio_kinit(&tempIovec, &tempUio2, buf2, 4, file2->of_offset, UIO_READ);
		result = VOP_READ(file2->of_vnode, &tempUio2);
		if(result != 0)
			return result;

		file2->of_offset = tempUio2.uio_offset;
		
		lock_release(file2->of_offsetlock);

		//write 4 bytes from file 1 to the meld file
		lock_acquire(file3->of_offsetlock);
		uio_kinit(&tempIovec, &writeUio, buf1, 4, file3->of_offset, UIO_WRITE);
		result = VOP_WRITE(file3->of_vnode, &writeUio);
		if(result != 0)
			return result;

		file3->of_offset = writeUio.uio_offset;
		
		lock_release(file3->of_offsetlock);

		
		//write 4 bytes from file 2 to the meld file
		lock_acquire(file3->of_offsetlock);
		uio_kinit(&tempIovec, &writeUio, buf2, 4, file3->of_offset, UIO_WRITE);
		result = VOP_WRITE(file3->of_vnode, &writeUio);
		if(result != 0)
			return result;

		file3->of_offset = writeUio.uio_offset;

		lock_release(file3->of_offsetlock);
	
		count += 4;
	}

	*retval = file3->of_offset;
	
	//closing files

	KASSERT(filetable_okfd(curproc->p_filetable, fd));

	filetable_placeat(curproc->p_filetable, NULL, fd, &file1);
	if(file1 == NULL)
		return ENOENT;

	openfile_decref(file1);

	filetable_placeat(curproc->p_filetable, NULL, fd, &file2);
	if(file2 == NULL)
		return ENOENT;

	openfile_decref(file2);

	filetable_placeat(curproc->p_filetable, NULL, fd, &file3);
	if(file3 == NULL)
		return ENOENT;
	
	openfile_decref(file3);

	kfree(kpathPn1);
	kfree(kpathPn2);
	kfree(kpathPn3);
	kfree(buf1);
	kfree(buf2);


	return 0;
}

