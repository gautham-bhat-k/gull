/*
 *  (c) Copyright 2016-2021 Hewlett Packard Enterprise Development Company LP.
 *
 *  This software is available to you under a choice of one of two
 *  licenses. You may choose to be licensed under the terms of the 
 *  GNU Lesser General Public License Version 3, or (at your option)  
 *  later with exceptions included below, or under the terms of the  
 *  MIT license (Expat) available in COPYING file in the source tree.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the
 *  Application containing code generated by the Library and added to the
 *  Application during this compilation process under terms of your choice,
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#include <unistd.h> // sleep
#include <random>
#include <limits>

#include <gtest/gtest.h>
#include "nvmm/memory_manager.h"
#include "common/crash_points.h"
#include "test_common/test.h"

using namespace nvmm;

// random number and string generator
std::random_device r;
std::default_random_engine e1(r());
uint64_t rand_uint64(uint64_t min=0, uint64_t max=std::numeric_limits<uint64_t>::max())
{
    std::uniform_int_distribution<uint64_t> uniform_dist(min, max);
    return uniform_dist(e1);
}

void Merge(PoolId pool_id) {
    EpochManager *em = EpochManager::GetInstance();
    em->Start();

    size_t size = 128*1024*1024LLU; // 128 MB

    MemoryManager *mm = MemoryManager::GetInstance();
    Heap *heap = NULL;

    // create a heap
    EXPECT_EQ(NO_ERROR, mm->CreateHeap(pool_id, size));

    // get the heap
    EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, heap->Open());

    // in unit of 64-byte:
    // [0, 8) has been allocated to the header
    // [4096, 8192) has been allocated to the merge bitmap
    uint64_t min_obj_size = heap->MinAllocSize();

    // merge at levels < max_zone_level-2
    // allocate 64 byte x 24, covering [8, 32)
    GlobalPtr ptr[24];
    for(int i=0; i<24; i++) {
        ptr[i]= heap->Alloc(min_obj_size);
    }
    // free 64 byte x 24
    for(int i=0; i<24; i++) {
        heap->Free(ptr[i]);
    }

    // before merge, allocate 1024 bytes
    GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
    EXPECT_EQ(32*min_obj_size, new_ptr.GetOffset());

    // merge
    heap->Merge();
}

// merge
TEST(EpochZoneHeapCrash, Merge)
{
    PoolId pool_id = 1;
    pid_t pid;
    EpochManager *em = EpochManager::GetInstance();

    // merge after 1
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 1");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        // NOTE: the merge did not start
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(48*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 2
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 2");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 3
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 3");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();
        // TODO: OnlineRecover should return false

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 4
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 4");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 5
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 5");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 6
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 6");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge during 7
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge during 7");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 8
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 8");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge during 9
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge during 9");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 10
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 10");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        // NOTE: because technically the merge succeeded at level 1 and reset current_merge_level
        // back to -1, the recovery procedure would not continue with merge at higher levels
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(48*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

    // merge after 11
    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        // set crash point
        CrashPoints::EnableCrashPoint("merge after 11");
        Merge(pool_id);
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // run online recovery
        heap->OnlineRecover();

        // after merge, allocate 1024 bytes
        // NOTE: because technically the merge succeeded at level 1 and reset current_merge_level
        // back to -1, the recovery procedure would not continue with merge at higher levels
        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
        EXPECT_EQ(48*min_obj_size, new_ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }
}

TEST(EpochZoneHeapCrash, GarbageCollection)
{
    PoolId pool_id = 1;
    pid_t pid;
    EpochManager *em = EpochManager::GetInstance();

    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        EpochManager *em = EpochManager::GetInstance();
        em->Start();

        size_t size = 128*1024*1024LLU; // 128 MB

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;

        // create a heap
        EXPECT_EQ(NO_ERROR, mm->CreateHeap(pool_id, size));

        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // in unit of 64-byte:
        // [0, 8) has been allocated to the header
        // [4096, 8192) has been allocated to the merge bitmap
        uint64_t min_obj_size = heap->MinAllocSize();

        GlobalPtr ptr;
        ptr = heap->Alloc(min_obj_size);
        EXPECT_EQ(8*min_obj_size, ptr.GetOffset());

        CrashPoints::EnableCrashPoint("alloc before set bitmap");
        ptr = heap->Alloc(min_obj_size);
        EXPECT_EQ(9*min_obj_size, ptr.GetOffset());
        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();
    }

    em->Stop();
    pid = fork();
    ASSERT_LE(0, pid);
    if (pid==0)
    {
        // child
        EpochManager *em = EpochManager::GetInstance();
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;

        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        // in unit of 64-byte:
        // [0, 8) has been allocated to the header
        // [4096, 8192) has been allocated to the merge bitmap
        uint64_t min_obj_size = heap->MinAllocSize();

        CrashPoints::EnableCrashPoint("alloc during split");
        GlobalPtr ptr;
        ptr = heap->Alloc(4096*min_obj_size);
        EXPECT_EQ(8192*min_obj_size, ptr.GetOffset());

        exit(0); // this will leak memory (see valgrind output)
    }
    else
    {
        // parent
        int status;
        std::cout << "Waiting for process " << pid << std::endl;
        waitpid(pid, &status, 0);
        em->Start();

        MemoryManager *mm = MemoryManager::GetInstance();
        Heap *heap = NULL;
        // get the heap
        EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
        EXPECT_EQ(NO_ERROR, heap->Open());

        uint64_t min_obj_size = heap->MinAllocSize();
        GlobalPtr ptr;

        // before recovery
        ptr = heap->Alloc(min_obj_size);
        // 9 was lost
        EXPECT_EQ(10*min_obj_size, ptr.GetOffset());

        // [8192, 163384) was lost during split
        ptr = heap->Alloc(4096*min_obj_size);
        EXPECT_EQ(16384*min_obj_size, ptr.GetOffset());
        ptr = heap->Alloc(4096*min_obj_size);
        EXPECT_EQ(20480*min_obj_size, ptr.GetOffset());

        // run online recovery
        heap->OfflineRecover();

        // after recovery
        ptr = heap->Alloc(min_obj_size);
        // 9 was recovered
        EXPECT_EQ(9*min_obj_size, ptr.GetOffset());

        // 8192 was recovered
        ptr = heap->Alloc(4096*min_obj_size);
        EXPECT_EQ(8192*min_obj_size, ptr.GetOffset());
        ptr = heap->Alloc(4096*min_obj_size);
        EXPECT_EQ(12288*min_obj_size, ptr.GetOffset());

        // destroy the heap
        EXPECT_EQ(NO_ERROR, heap->Close());
        delete heap;
        EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
        EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
    }

}

int main(int argc, char** argv)
{
    InitTest(nvmm::trace, false);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
