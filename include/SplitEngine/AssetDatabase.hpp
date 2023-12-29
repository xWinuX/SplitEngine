#pragma once

#include "ErrorHandler.hpp"

#include <format>
#include <functional>
#include <ranges>
#include <unordered_map>

namespace SplitEngine
{
	class AssetDatabase;

	template<class T>
	struct AssetHandle
	{
			friend AssetDatabase;

		public:
			AssetHandle() = default;

			T* operator->()
			{
				if (_asset == nullptr) { _asset = _assetDatabase->GetAsset<T>(*this); }
				return _asset;
			}

		private:
			explicit AssetHandle(AssetDatabase* assetDatabase, uint64_t id) :
				_assetDatabase(assetDatabase),
				_id(id)
			{}

			T*             _asset         = nullptr;
			AssetDatabase* _assetDatabase = nullptr;
			uint64_t       _id            = -1;
	};

	class AssetDatabase
	{
		public:
			~AssetDatabase()
			{
				for (auto& deletionFunction : std::ranges::reverse_view(_assetDeletionList)) { deletionFunction(); }
			}

			template<class T, class TKey>
			[[nodiscard]] AssetHandle<T> CreateAsset(TKey key, T::CreateInfo createInfo)
			{
				T* pointer                                 = new T(createInfo);
				GetAssets<T>()[static_cast<uint64_t>(key)] = pointer;

				_assetDeletionList.push_back([pointer] {
					delete pointer;
				});

				return AssetHandle<T>(this, static_cast<uint64_t>(key));
			}

			template<typename T>
			std::unordered_map<uint64_t, T*>& GetAssets()
			{
				static std::unordered_map<uint64_t, T*> map;
				return map;
			}

			template<class T>
			T* GetAsset(AssetHandle<T> assetHandle)
			{
				return (T*) GetAssets<T>().at(assetHandle._id);
			}

			template<typename T, typename TKey>
			T* GetAsset(TKey key)
			{
				return GetAssets<T>().at(static_cast<uint64_t>(key));
			}

		private:
			std::vector<std::function<void()>> _assetDeletionList;
	};

}
