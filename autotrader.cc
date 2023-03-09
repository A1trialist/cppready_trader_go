// Copyright 2021 Optiver Asia Pacific Pty. Ltd.
//
// This file is part of Ready Trader Go.
//
//     Ready Trader Go is free software: you can redistribute it and/or
//     modify it under the terms of the GNU Affero General Public License
//     as published by the Free Software Foundation, either version 3 of
//     the License, or (at your option) any later version.
//
//     Ready Trader Go is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU Affero General Public License for more details.
//
//     You should have received a copy of the GNU Affero General Public
//     License along with Ready Trader Go.  If not, see
//     <https://www.gnu.org/licenses/>.
#include <array>

#include <boost/asio/io_context.hpp>

#include <ready_trader_go/logging.h>

#include "autotrader.h"

using namespace ReadyTraderGo;

RTG_INLINE_GLOBAL_LOGGER_WITH_CHANNEL(LG_AT, "AUTO")

constexpr signed long int LOT_SIZE = 10;
constexpr signed long int POSITION_LIMIT = 100;

constexpr signed long int ACTIVE_ORDERS_LIMIT = 10;
constexpr signed long int ACTIVE_VOLUME_LIMIT = 200;
constexpr signed long int ORDER_OFFSETS[] = {100, 200, 300};
constexpr signed long int ORDER_SHARE[] = {3, 2, 6};

constexpr signed long int TICK_SIZE_IN_CENTS = 100;
constexpr signed long int MIN_BId_NEARST_TICK = (MINIMUM_BID + TICK_SIZE_IN_CENTS) / TICK_SIZE_IN_CENTS * TICK_SIZE_IN_CENTS;
constexpr signed long int MAX_ASK_NEAREST_TICK = MAXIMUM_ASK / TICK_SIZE_IN_CENTS * TICK_SIZE_IN_CENTS;

AutoTrader::AutoTrader(boost::asio::io_context& context) : BaseAutoTrader(context)
{
}

void AutoTrader::DisconnectHandler()
{
    BaseAutoTrader::DisconnectHandler();
    RLOG(LG_AT, LogLevel::LL_INFO) << "execution connection lost";
}

void AutoTrader::ErrorMessageHandler(unsigned long clientOrderId,
                                     const std::string& errorMessage)
{
    RLOG(LG_AT, LogLevel::LL_INFO) << "error with order " << clientOrderId << ": " << errorMessage;
    if (clientOrderId != 0 && ((mAsks.count(clientOrderId) == 1) || (mBids.count(clientOrderId) == 1)))
    {
        OrderStatusMessageHandler(clientOrderId, 0, 0, 0);
    }
}

void AutoTrader::HedgeFilledMessageHandler(unsigned long clientOrderId,
                                           unsigned long price,
                                           unsigned long volume)
{
    RLOG(LG_AT, LogLevel::LL_INFO) << "hedge order " << clientOrderId << " filled for " << volume
                                   << " lots at $" << price << " average price in cents";
    
    if (mHedgeBids.find (clientOrderId) != mHedgeBids.end ()) {
        mFutPosition += volume;
        mFutProfit -= volume * price;
    } else if (mHedgeAsks.find (clientOrderId) != mHedgeAsks.end ()) {
        mFutPosition -= volume;
        mFutProfit += volume * price;
    }

}

void AutoTrader::OrderBookMessageHandler(Instrument instrument,
                                         unsigned long sequenceNumber,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT>& askPrices,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT>& askVolumes,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT>& bidPrices,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT>& bidVolumes)
{
    RLOG(LG_AT, LogLevel::LL_INFO) << "order book received for " << instrument << " instrument"
                                   << ": ask prices: " << askPrices[0]
                                   << "; ask volumes: " << askVolumes[0]
                                   << "; bid prices: " << bidPrices[0]
                                   << "; bid volumes: " << bidVolumes[0];

    /*
    if (instrument == Instrument::FUTURE)
    {
        unsigned long priceAdjustment = - (mPosition / LOT_SIZE) * TICK_SIZE_IN_CENTS;
        unsigned long newAskPrice = (askPrices[0] != 0) ? askPrices[0] + priceAdjustment : 0;
        unsigned long newBidPrice = (bidPrices[0] != 0) ? bidPrices[0] + priceAdjustment : 0;

        if (mAskId != 0 && newAskPrice != 0 && newAskPrice != mAskPrice)
        {
            SendCancelOrder(mAskId);
            mAskId = 0;
        }
        if (mBidId != 0 && newBidPrice != 0 && newBidPrice != mBidPrice)
        {
            SendCancelOrder(mBidId);
            mBidId = 0;
        }

        if (mAskId == 0 && newAskPrice != 0 && mPosition > -POSITION_LIMIT)
        {
            mAskId = mNextMessageId++;
            mAskPrice = newAskPrice;
            SendInsertOrder(mAskId, Side::SELL, newAskPrice, LOT_SIZE, Lifespan::GOOD_FOR_DAY);
            mAsks.emplace(mAskId);
        }
        if (mBidId == 0 && newBidPrice != 0 && mPosition < POSITION_LIMIT)
        {
            mBidId = mNextMessageId++;
            mBidPrice = newBidPrice;
            SendInsertOrder(mBidId, Side::BUY, newBidPrice, LOT_SIZE, Lifespan::GOOD_FOR_DAY);
            mBids.emplace(mBidId);
        }
    } 
    */
}

void AutoTrader::OrderFilledMessageHandler(unsigned long clientOrderId,
                                           unsigned long price,
                                           unsigned long volume)
{
    RLOG(LG_AT, LogLevel::LL_INFO) << "order " << clientOrderId << " filled for " << volume
                                   << " lots at $" << price << " cents";
    if (mAsks.count(clientOrderId) == 1)
    {
        mPosition -= (long)volume;
        /*
            self.hedge_bid_id = next(self.order_ids)
            self.send_hedge_order(self.hedge_bid_id, Side.BId, MAX_ASK_NEAREST_TICK, volume)
            self.hedge_bids.add(self.hedge_bid_id)

            self.etf_profit += volume * price
        */
        mHedgeBidId = mNextMessageId++;
        SendHedgeOrder(mHedgeBidId, Side::BUY, MAX_ASK_NEAREST_TICK, volume);
        mHedgeBids.insert (mHedgeBidId);
        
        mETFProfit += volume * price;
    }
    else if (mBids.count(clientOrderId) == 1)
    {
        mPosition += (long)volume;
        /*
            self.hedge_ask_id = next(self.order_ids)
            self.send_hedge_order(self.hedge_ask_id, Side.ASK, MIN_BId_NEAREST_TICK, volume)
            self.hedge_asks.add(self.hedge_ask_id)

            self.etf_profit -= volume * price
        */
        mHedgeAskId = mNextMessageId++;
        SendHedgeOrder(mHedgeAskId, Side::SELL, MIN_BId_NEARST_TICK, volume);
        mHedgeAsks.insert (mHedgeAskId);
        
        mETFProfit -= volume * price;
    }
}

void AutoTrader::OrderStatusMessageHandler(unsigned long clientOrderId,
                                           unsigned long fillVolume,
                                           unsigned long remainingVolume,
                                           signed long fees)
{
    if (remainingVolume == 0)
    {
        /*
        if (clientOrderId == mAskId)
        {
            mAskId = 0;
        }
        else if (clientOrderId == mBidId)
        {
            mBidId = 0;
        }
        */

        /*

        if client_order_id in self.bids:
            self.bids.discard(client_order_id)
            self.active_bid -= fill_volume
        elif client_order_id in self.asks:
            self.asks.discard(client_order_id)
            self.active_ask -= fill_volume
        self.active_orders -= 1
        self.active_volume -= fill_volume

        print(f"etf_profit={self.etf_profit + self.position * self.etf_mid} fut_profit={self.future_profit + self.fut_position * self.fut_mid}")

        */
        if (mBids.count (clientOrderId) == 1) {
            mBids.erase (clientOrderId);
            mActiveBid -= fillVolume;
        } else if (mAsks.count (clientOrderId) == 1) {
            mAsks.erase (clientOrderId);
            mActiveAsk -= fillVolume;
        }
        mActiveOrders -= 1;
        mActiveVolume -= fillVolume;

        printf ("etf_profit=%ld fut_profit=%ld\n", 
            mETFProfit + mPosition * mETFMid,
            mFutProfit + mFutPosition * mFutMid
        );
    }

}

void AutoTrader::TradeTicksMessageHandler(Instrument instrument,
                                          unsigned long sequenceNumber,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT>& askPrices,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT>& askVolumes,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT>& bidPrices,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT>& bidVolumes)
{
    RLOG(LG_AT, LogLevel::LL_INFO) << "trade ticks received for " << instrument << " instrument"
                                   << ": ask prices: " << askPrices[0]
                                   << "; ask volumes: " << askVolumes[0]
                                   << "; bid prices: " << bidPrices[0]
                                   << "; bid volumes: " << bidVolumes[0];
    if (instrument == Instrument::FUTURE) {
        mFutOrder = false;
        if (bidPrices[0] == 0 && askPrices[0] == 0) {
            return;
        } else if (bidPrices[0] == 0) {
            mFutMid = askPrices[0];
        } else if (askPrices[0] == 0) {
            mFutMid = bidPrices[0];
        } else {
            mFutMid = (bidPrices[0] + askPrices[0]) / 200 * 100;
        }
        // # if time.time() - self.lst_fut > 30.0:
        // #     if self.position + self.fut_position > 0:
        // #         self.hedge_ask_id = next(self.order_ids)
        // #         self.send_hedge_order(self.hedge_ask_id, Side.ASK, MIN_BId_NEAREST_TICK, (self.position + self.fut_position))
        // #         self.hedge_asks.add(self.hedge_ask_id)

        // #     elif self.position + self.fut_position < 0:
        // #         self.hedge_bid_id = next(self.order_ids)
        // #         self.send_hedge_order(self.hedge_bid_id, Side.BId, MAX_ASK_NEAREST_TICK, -(self.position + self.fut_position))
        // #         self.hedge_bids.add(self.hedge_bid_id)
        // #     self.lst_fut = time.time()
    } else if (instrument == Instrument::ETF) {
        if (bidPrices[0] == 0 && askPrices[0] == 0) {
            return;
        } else if (bidPrices[0] == 0) {
            mETFMid = askPrices[0];
        } else if (askPrices[0] == 0) {
            mETFMid = bidPrices[0];
        } else {
            mETFMid = (bidPrices[0] + askPrices[0]) / 200 * 100;
        }

        if (mFutOrder == true) {
            mFutOrder = false;
            printf ("etf_mid: %ld\n, fut_mid: %ld\n", mETFMid, mFutMid);
            if (mETFMid < mFutMid + 300) {
                signed long available = std::min(60L, std::min(mPosition - mActiveAsk + POSITION_LIMIT, ACTIVE_VOLUME_LIMIT - mAskVolume));
                for (int i = 0;i < 3;i++) {
                    if (mActiveOrders < ACTIVE_ORDERS_LIMIT) {
                        signed long volume = available / ORDER_SHARE[i];
                        if (volume <= 0) continue;
                        mAskId = mOrderIds++;
                        SendInsertOrder (mAskId, Side::SELL, mETFMid + ORDER_OFFSETS[i], volume, Lifespan::GOOD_FOR_DAY);
                        mAsks.insert (mAskId);
                        mActiveVolume += volume;
                        mActiveOrders += 1;
                        mActiveAsk += volume;
                    }
                }
            } 
            if (mETFMid + 300 > mFutMid) {
                signed long available = std::min(60L, std::min(-mPosition + POSITION_LIMIT - mActiveBid, ACTIVE_VOLUME_LIMIT - mActiveVolume));
                for (int i = 0;i < 3;i++) {
                    if (mActiveOrders < ACTIVE_ORDERS_LIMIT) {
                        signed long volume = available / ORDER_SHARE[i];
                        if (volume <= 0) continue;
                        mBidId = mOrderIds++;
                        SendInsertOrder (mBidId, Side::BUY, mETFMid + 100 - ORDER_OFFSETS[i], volume, Lifespan::GOOD_FOR_DAY);
                        mBids.insert (mBidId);
                        mActiveVolume += volume;
                        mActiveOrders += 1;
                        mActiveBid += volume;
                    }
                }
            }
        }
    }
}
