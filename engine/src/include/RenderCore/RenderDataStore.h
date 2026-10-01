#pragma once

#include <cassert>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "RenderCore/StableTypeId.h"

class IRenderDataChannel
{
public:
  virtual ~IRenderDataChannel() = default;
  virtual StableTypeId elementType() const = 0;
  virtual std::unique_ptr<IRenderDataChannel> Clone() const = 0;
  virtual void AppendTo(class RenderDataStore& destination) const = 0;
};

template<typename T>
class RenderDataChannel final : public IRenderDataChannel
{
public:
  StableTypeId elementType() const override
  {
    return typeId<T>();
  }

  std::unique_ptr<IRenderDataChannel> Clone() const override
  {
    return std::make_unique<RenderDataChannel<T>>(*this);
  }

  void AppendTo(RenderDataStore& destination) const override;

  void Append(const T& value)
  {
    values.push_back(value);
  }

  void Append(T&& value)
  {
    values.push_back(std::move(value));
  }

  const std::vector<T>& Items() const
  {
    return values;
  }

private:
  std::vector<T> values;
};

class RenderDataStore
{
public:
  RenderDataStore() = default;

  RenderDataStore(const RenderDataStore& other)
  {
    for (const auto& [type, channel] : other.channels)
    {
      channels.emplace(type, channel->Clone());
    }
  }

  RenderDataStore& operator=(const RenderDataStore& other)
  {
    if (this == &other)
    {
      return *this;
    }

    RenderDataStore copy(other);
    channels = std::move(copy.channels);
    return *this;
  }

  RenderDataStore(RenderDataStore&&) noexcept = default;
  RenderDataStore& operator=(RenderDataStore&&) noexcept = default;

  void AppendFrom(const RenderDataStore& other)
  {
    for (const auto& [type, channel] : other.channels)
    {
      (void)type;
      channel->AppendTo(*this);
    }
  }

  template<typename T>
  RenderDataChannel<T>& GetOrCreate()
  {
    const StableTypeId channelType = typeId<T>();
    const auto iterator = channels.find(channelType);
    if (iterator != channels.end())
    {
      auto* typedChannel = dynamic_cast<RenderDataChannel<T>*>(iterator->second.get());
      assert(typedChannel != nullptr && "RenderDataStore channel type mismatch");
      return *typedChannel;
    }

    auto channel = std::make_unique<RenderDataChannel<T>>();
    RenderDataChannel<T>* channelPtr = channel.get();
    channels.emplace(channelType, std::move(channel));
    return *channelPtr;
  }

  template<typename T>
  void Append(const T& value)
  {
    GetOrCreate<T>().Append(value);
  }

  template<typename T>
  void Append(T&& value)
  {
    GetOrCreate<T>().Append(std::move(value));
  }

  template<typename T>
  bool Contains() const
  {
    return channels.find(typeId<T>()) != channels.end();
  }

  template<typename T>
  const std::vector<T>& Read() const
  {
    const auto iterator = channels.find(typeId<T>());
    if (iterator == channels.end())
    {
      static const std::vector<T> empty;
      return empty;
    }

    const auto* typedChannel = dynamic_cast<const RenderDataChannel<T>*>(iterator->second.get());
    assert(typedChannel != nullptr && "RenderDataStore channel type mismatch");
    return typedChannel->Items();
  }

private:
  std::unordered_map<StableTypeId, std::unique_ptr<IRenderDataChannel>> channels;
};

template<typename T>
void RenderDataChannel<T>::AppendTo(RenderDataStore& destination) const
{
  for (const T& value : values)
  {
    destination.Append(value);
  }
}
