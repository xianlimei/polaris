/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * ident	"@(#)UserStackRecord.java	1.1	06/02/16 SMI"
 */
package org.opensolaris.os.dtrace;

import java.util.*;
import java.io.*;
import java.beans.*;

/**
 * A value generated by the DTrace {@code ustack()} or {@code jstack()}
 * action.
 * <p>
 * Immutable.  Supports persistence using {@link java.beans.XMLEncoder}.
 *
 * @author Tom Erickson
 */
public final class UserStackRecord implements StackValueRecord,
       Serializable, Comparable <UserStackRecord>
{
    static final long serialVersionUID = -4195269026915862308L;

    static {
	try {
	    BeanInfo info = Introspector.getBeanInfo(UserStackRecord.class);
	    PersistenceDelegate persistenceDelegate =
		    new DefaultPersistenceDelegate(
		    new String[] {"processID", "stackFrames", "rawStackData"})
	    {
		/*
		 * Need to prevent DefaultPersistenceDelegate from using
		 * overridden equals() method, resulting in a
		 * StackOverFlowError.  Revert to PersistenceDelegate
		 * implementation.  See
		 * http://forum.java.sun.com/thread.jspa?threadID=
		 * 477019&tstart=135
		 */
		protected boolean
		mutatesTo(Object oldInstance, Object newInstance)
		{
		    return (newInstance != null && oldInstance != null &&
			    oldInstance.getClass() == newInstance.getClass());
		}
	    };
	    BeanDescriptor d = info.getBeanDescriptor();
	    d.setValue("persistenceDelegate", persistenceDelegate);
	} catch (IntrospectionException e) {
	    System.out.println(e);
	}
    }

    private transient KernelStackRecord stackRecord;
    /** @serial */
    private final int processID;

    /**
     * Called by native code.
     */
    private
    UserStackRecord(int pid, byte[] rawBytes)
    {
	stackRecord = new KernelStackRecord(rawBytes);
	processID = pid;
	validate();
    }

    /**
     * Creates a {@code UserStackRecord} with the given stack frames,
     * user process ID, and raw stack data.  Supports XML persistence.
     *
     * @param frames array of human-readable stack frames, copied so
     * that later modifying the given frames array will not affect this
     * {@code UserStackRecord}; may be {@code null} or empty to indicate
     * that the raw stack data was not converted to human-readable stack
     * frames (see {@link StackValueRecord#getStackFrames()})
     * @param pid non-negative user process ID
     * @param rawBytes array of raw bytes used to represent this stack
     * value in the native DTrace library, needed to distinguish stacks
     * that have the same display value but are considered distinct by
     * DTrace; copied so that later modifying the given array will not
     * affect this {@code UserStackRecord}
     * @throws NullPointerException if the given array of raw bytes is
     * {@code null} or if any element of the {@code frames} array is
     * {@code null}
     * @throws IllegalArgumentException if the given process ID is
     * negative
     */
    public
    UserStackRecord(int pid, StackFrame[] frames, byte[] rawBytes)
    {
	stackRecord = new KernelStackRecord(frames, rawBytes);
	processID = pid;
	validate();
    }

    private void
    validate()
    {
	if (processID < 0) {
	    throw new IllegalArgumentException("process ID is negative");
	}
    }

    public StackFrame[]
    getStackFrames()
    {
	return stackRecord.getStackFrames();
    }

    void
    setStackFrames(StackFrame[] frames)
    {
	stackRecord.setStackFrames(frames);
    }

    /**
     * Gets the native DTrace representation of this record's stack as
     * an array of raw bytes.  The raw bytes include the process ID and
     * are used in {@link #equals(Object o) equals()} and {@link
     * #compareTo(UserStackRecord r) compareTo()} to test equality and
     * to determine the natural ordering of user stack records.
     *
     * @return the native DTrace library's internal representation of
     * this record's stack as a non-null array of bytes
     */
    public byte[]
    getRawStackData()
    {
	return stackRecord.getRawStackData();
    }

    /**
     * Gets the raw bytes used to represent this record's stack value in
     * the native DTrace library.  To get a human-readable
     * representation, call {@link #toString()}.
     *
     * @return {@link #getRawStackData()}
     */
    public Object
    getValue()
    {
	return stackRecord.getValue();
    }

    /**
     * Gets the process ID associated with this record's user stack.
     *
     * @return non-negative pid
     */
    public int
    getProcessID()
    {
	return processID;
    }

    public List <StackFrame>
    asList()
    {
	return stackRecord.asList();
    }

    /**
     * Gets a {@code KernelStackRecord} view of this record.
     *
     * @return non-null {@code KernelStackRecord} view of this record
     */
    public KernelStackRecord
    asKernelStackRecord()
    {
	return stackRecord;
    }

    /**
     * Compares the specified object with this {@code UserStackRecord}
     * for equality.  Returns {@code true} if and only if the specified
     * object is also a {@code UserStackRecord} and both stacks have the
     * same raw stack data (including process ID).
     * <p>
     * This implementation first checks if the specified object is this
     * {@code UserStackRecord}.  If so, it returns {@code true}.
     *
     * @return {@code true} if and only if the specified object is also
     * a {@code UserStackRecord} and both stacks have the same raw stack
     * data (including process ID)
     */
    @Override
    public boolean
    equals(Object o)
    {
	if (o == this) {
	    return true;
	}
	if (o instanceof UserStackRecord) {
	    UserStackRecord r = (UserStackRecord)o;
	    // raw stack data includes the process ID, but the process
	    // ID passed to the constructor is not validated against the
	    // raw stack data; probably best for this class to test all
	    // of its state without making assumptions
	    return ((processID == r.processID) &&
		    stackRecord.equals(r.stackRecord));
	}
	return false;
    }

    /**
     * Overridden to ensure that equal instances have equal hash codes.
     */
    @Override
    public int
    hashCode()
    {
	return (stackRecord.hashCode() + processID);
    }

    /**
     * Compares this record with the given {@code UserStackRecord}.
     * Compares process ID first, then if those are equal, compares the
     * views returned by {@link #asKernelStackRecord()}.  The {@code
     * compareTo()} method is compatible with {@link #equals(Object o)
     * equals()}.
     * <p>
     * This implementation first checks if the specified object is this
     * {@code UserStackRecord}.  If so, it returns {@code 0}.
     *
     * @return -1, 0, or 1 as this record is less than, equal to, or
     * greater than the given record
     */
    public int
    compareTo(UserStackRecord r)
    {
	if (r == this) {
	    return 0;
	}

	int cmp = 0;
	cmp = ((processID < r.processID) ? -1 :
		((processID > r.processID) ? 1 : 0));
	if (cmp == 0) {
	    cmp = stackRecord.compareTo(r.stackRecord);
	}
	return cmp;
    }

    /**
     * Serialize this {@code UserStackRecord} instance.
     *
     * @serialData Serialized fields are emitted, followed first by this
     * record's stack frames as an array of type {@link String}, then by
     * this record's raw stack data as an array of bytes.
     */
    private void
    writeObject(ObjectOutputStream s) throws IOException
    {
	s.defaultWriteObject();
	s.writeObject(stackRecord.getStackFrames());
	s.writeObject(stackRecord.getRawStackData());
    }

    private void
    readObject(ObjectInputStream s) throws IOException, ClassNotFoundException
    {
	s.defaultReadObject();
	try {
	    StackFrame[] frames = (StackFrame[])s.readObject();
	    byte[] rawBytes = (byte[])s.readObject();
	    // defensively copies stack frames and raw bytes
	    stackRecord = new KernelStackRecord(frames, rawBytes);
	} catch (Exception e) {
	    throw new InvalidObjectException(e.getMessage());
	}
	// check class invariants
	try {
	    validate();
	} catch (Exception e) {
	    throw new InvalidObjectException(e.getMessage());
	}
    }

    /**
     * Gets the {@link KernelStackRecord#toString() string
     * representation} of the view returned by {@link
     * #asKernelStackRecord()} preceded by the user process ID on its
     * own line.  The process ID is in the format {@code process ID:
     * pid} (where <i>pid</i> is a decimal integer) and is indented by
     * the same number of spaces as the stack frames.  The exact format
     * is subject to change.
     */
    public String
    toString()
    {
	StringBuffer buf = new StringBuffer();
	final int stackindent = KernelStackRecord.STACK_INDENT;
	int i;
	buf.append('\n');
	for (i = 0; i < KernelStackRecord.STACK_INDENT; ++i) {
	    buf.append(' ');
	}
	buf.append("process ID: ");
	buf.append(processID);
	buf.append(stackRecord.toString()); // starts with newline
	return buf.toString();
    }
}
