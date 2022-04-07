/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "gtest/gtest.h"

#include "../stage_data.h"

template <uint32_t num_stages, uint32_t num_ops>
class StageDataTest : public ::testing::Test {

    protected:
    StageData *sd;
    std::array<Op*, num_ops> *ops;

    virtual void SetUp() {
        sd = new StageData(0, "Test Stage", num_stages);
        ops = new std::array<Op*, num_ops>;

        for (uint32_t i = 0; i < ops->size(); ++i) {
            ops->at(i) = new Op;
            ops->at(i)->op_num = i;
        }
    }

    virtual void TearDown() {
        delete sd;

        for (uint32_t i = 0; i < ops->size(); ++i) {
            delete ops->at(i);
        }
        delete ops;
    }

    void insert_ops_helper(uint32_t num_ops_insert) {
        for (uint32_t i = 0; i < num_ops_insert; ++i) {
            sd->insert(ops->at(i));
        }
    }

    void EmptyTest(uint32_t stage_width) {
        ASSERT_EQ("Test Stage", sd->name);
        ASSERT_EQ(stage_width, sd->ops.size());
        ASSERT_EQ(0, sd->op_count);

        for (auto& op : sd->ops) {
            ASSERT_EQ(NULL, op);
        }
    }

    void InsertTest(uint32_t stage_width, uint32_t num_ops_insert) {
        ASSERT_EQ("Test Stage", sd->name);
        ASSERT_EQ(stage_width, sd->ops.size());
        ASSERT_EQ(num_ops_insert, sd->op_count);

        for (uint32_t i = 0; i < num_ops_insert; ++i) {
            ASSERT_NE(nullptr, sd->ops.at(i));
            ASSERT_EQ(i, sd->ops.at(i)->op_num);
        }

        for (uint32_t i = num_ops_insert; i < stage_width; ++i) {
            ASSERT_EQ(nullptr, sd->ops.at(i));
        }
    }
};

using StageDataTest_4_0=StageDataTest<4, 0>;
using StageDataTest_4_5=StageDataTest<4, 5>;
using StageDataTest_12_0=StageDataTest<12, 0>;

TEST_F(StageDataTest_4_0, EmptyStageData4) {
    EmptyTest(4);
}

TEST_F(StageDataTest_12_0, EmptyStageData12) {
    EmptyTest(12);
}

TEST_F(StageDataTest_4_5, Insert1StageData4) {
    insert_ops_helper(1);
    InsertTest(4, 1);
}

TEST_F(StageDataTest_4_5, Insert2StageData4) {
    insert_ops_helper(2);
    InsertTest(4, 2);
}

TEST_F(StageDataTest_4_5, Insert4StageData4) {
    insert_ops_helper(4);
    InsertTest(4, 4);
}

TEST_F(StageDataTest_4_5, Insert5StageData4) {
    insert_ops_helper(4);

    ASSERT_DEATH({
            sd->insert(ops->at(4));
        }, "Error on line .* of Insert5StageData4");
}

TEST_F(StageDataTest_4_5, ResetStageData4) {
    insert_ops_helper(4);
    sd->reset();
    EmptyTest(4);
}
