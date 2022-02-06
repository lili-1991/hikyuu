/*
 * ILowLine.cpp
 *
 *  Copyright (c) 2019 hikyuu.org
 *
 *  Created on: 2016年4月2日
 *      Author: fasiondog
 */

#include "hikyuu/utilities/thread/ThreadPool.h"
#include "hikyuu/global/GlobalTaskGroup.h"
#include "ILowLine.h"

#if HKU_SUPPORT_SERIALIZATION
BOOST_CLASS_EXPORT(hku::ILowLine)
#endif

namespace hku {

ILowLine::ILowLine() : IndicatorImp("LLV", 1) {
    setParam<int>("n", 20);
}

ILowLine::~ILowLine() {}

bool ILowLine::check() {
    return haveIndParam("n") || getParam<int>("n") >= 0;
}

void ILowLine::_calculate(const Indicator& ind) {
    SPEND_TIME(ILowLine__calculate);
    size_t total = ind.size();
    if (0 == total) {
        m_discard = 0;
        return;
    }

    if (ind.discard() >= total) {
        m_discard = total;
        return;
    }

    m_discard = ind.discard();
    if (1 == total) {
        if (0 == m_discard) {
            _set(ind[0], 0);
        }
        return;
    }

    int n = getParam<int>("n");
    if (n <= 0) {
        n = total - m_discard;
    } else if (n > total) {
        n = total;
    }

    size_t startPos = m_discard;
    size_t first_end = startPos + n >= total ? total : startPos + n;

    price_t min = ind[startPos];
    size_t pre_pos = startPos;
    for (size_t i = startPos; i < first_end; i++) {
        if (ind[i] <= min) {
            min = ind[i];
            pre_pos = i;
        }
        _set(min, i);
    }

    for (size_t i = first_end; i < total; i++) {
        size_t j = i + 1 - n;
        if (pre_pos < j) {
            pre_pos = j;
            min = ind[j];
            for (size_t j = pre_pos + 1; j <= i; j++) {
                if (ind[j] <= min) {
                    min = ind[j];
                    pre_pos = j;
                }
            }
        } else {
            if (ind[i] <= min) {
                min = ind[i];
                pre_pos = i;
            }
        }
        _set(min, i);
    }
}

void ILowLine::_dyn_calculate(const Indicator& data) {
    SPEND_TIME(ILowLine__dyn_calculate);
    const auto& ind_param = getIndParamImp("n");
    HKU_CHECK(ind_param->size() == data.size(), "ind_param->size()={}, data.size()={}!",
              ind_param->size(), data.size());
    m_discard = data.discard();
    auto tg = getGlobalTaskGroup();
    std::vector<std::future<price_t>> tasks;
    size_t total = data.size();
    for (size_t i = data.discard(); i < total; i++) {
        size_t step = size_t(ind_param->get(i));
        auto step_data = _get_one_step(data, i, 0, step);
        tasks.push_back(tg->submit([=]() {
            size_t len = step_data.size();
            HKU_IF_RETURN(len == 0, price_t(Null<price_t>()));
            price_t min_val = step_data[0];
            for (size_t i = 0; i < len; i++) {
                if (step_data[i] < min_val) {
                    min_val = step_data[i];
                }
            }
            return min_val;
        }));
    }
    for (size_t i = data.discard(); i < total; i++) {
        _set(tasks[i - data.discard()].get(), i, 0);
    }
}

Indicator HKU_API LLV(int n = 20) {
    IndicatorImpPtr p = make_shared<ILowLine>();
    p->setParam<int>("n", n);
    return Indicator(p);
}

Indicator HKU_API LLV(const IndParam& n) {
    IndicatorImpPtr p = make_shared<ILowLine>();
    p->setIndParam("n", n);
    return Indicator(p);
}

Indicator HKU_API LLV(const Indicator& ind, int n = 20) {
    return LLV(n)(ind);
}

Indicator HKU_API LLV(const Indicator& ind, const IndParam& n) {
    return LLV(n)(ind);
}

Indicator HKU_API LLV(const Indicator& ind, const Indicator& n) {
    return LLV(IndParam(n))(ind);
}

} /* namespace hku */
