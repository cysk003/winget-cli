// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include <mutex>
#include <winget/RepositorySource.h>
#include "CatalogPackage.h"
#include "CatalogPackage.g.cpp"
#include "PackageCatalog.h"
#include "PackageVersionInfo.h"
#include "PackageVersionId.h"
#include "PackageInstallerInstalledStatus.h"
#include "CheckInstalledStatusResult.h"
#include <ComContext.h>
#include <Workflows/ManifestComparator.h>
#include <wil\cppwinrt_wrl.h>
#include <winget/PinningData.h>
#include <winget/PackageVersionSelection.h>


namespace winrt::Microsoft::Management::Deployment::implementation
{
    void CatalogPackage::Initialize(
        ::AppInstaller::Repository::Source source,
        std::shared_ptr<::AppInstaller::Repository::ICompositePackage> package)
    {
        m_source = std::move(source);
        m_package = std::move(package);
    }
    hstring CatalogPackage::Id()
    {
        return winrt::to_hstring(m_package->GetProperty(::AppInstaller::Repository::PackageProperty::Id).get());
    }
    hstring CatalogPackage::Name()
    {
        return winrt::to_hstring(m_package->GetProperty(::AppInstaller::Repository::PackageProperty::Name));
    }
    Microsoft::Management::Deployment::PackageVersionInfo CatalogPackage::InstalledVersion()
    {
        std::call_once(m_installedVersionOnceFlag,
            [&]()
            {
                std::shared_ptr<::AppInstaller::Repository::IPackageVersion> installedVersion = GetInstalledVersion(m_package);
                if (installedVersion)
                {
                    auto installedVersionImpl = winrt::make_self<wil::details::module_count_wrapper<
                        winrt::Microsoft::Management::Deployment::implementation::PackageVersionInfo>>();
                    installedVersionImpl->Initialize(std::move(installedVersion));
                    m_installedVersion = *installedVersionImpl;
                }
            });
        return m_installedVersion;
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Management::Deployment::PackageVersionId> CatalogPackage::AvailableVersions()
    {
        std::call_once(m_availableVersionsOnceFlag,
            [&]()
            {
                // Vector hasn't been populated yet.
                for (auto const& versionKey : ::AppInstaller::Repository::GetAllAvailableVersions(m_package)->GetVersionKeys())
                {
                    auto packageVersionId = winrt::make_self<wil::details::module_count_wrapper<
                        winrt::Microsoft::Management::Deployment::implementation::PackageVersionId>>();
                    packageVersionId->Initialize(versionKey);
                    m_availableVersions.Append(*packageVersionId);
                }
            });
        return m_availableVersions.GetView();
    }

    void CatalogPackage::InitializeLatestApplicableVersion()
    {
        std::call_once(m_latestApplicableVersionOnceFlag,
            [&]()
            {
                using namespace AppInstaller::Pinning;

                auto availableVersions = AppInstaller::Repository::GetAvailableVersionsForInstalledVersion(m_package);
                auto installedVersion = AppInstaller::Repository::GetInstalledVersion(m_package);

                PinningData pinningData{ PinningData::Disposition::ReadOnly };
                auto evaluator = pinningData.CreatePinStateEvaluator(PinBehavior::ConsiderPins, installedVersion);

                AppInstaller::CLI::Execution::COMContext context;
                AppInstaller::Repository::IPackageVersion::Metadata installationMetadata =
                    installedVersion ? installedVersion->GetMetadata() : AppInstaller::Repository::IPackageVersion::Metadata{};
                AppInstaller::CLI::Workflow::ManifestComparator manifestComparator{ context, installationMetadata };

                std::shared_ptr<AppInstaller::Repository::IPackageVersion> latestApplicableVersion;
                auto availableVersionKeys = availableVersions->GetVersionKeys();
                for (const auto& availableVersionKey : availableVersionKeys)
                {
                    auto availableVersion = availableVersions->GetVersion(availableVersionKey);

                    if (installedVersion && !evaluator.IsUpdate(availableVersion))
                    {
                        // Version too low or different channel for upgrade
                        continue;
                    }

                    if (evaluator.EvaluatePinType(availableVersion) != AppInstaller::Pinning::PinType::Unknown)
                    {
                        // Pinned
                        continue;
                    }

                    auto manifestComparatorResult = manifestComparator.GetPreferredInstaller(availableVersion->GetManifest());
                    if (!manifestComparatorResult.installer.has_value())
                    {
                        // No applicable installer
                        continue;
                    }

                    latestApplicableVersion = availableVersion;
                    if (installedVersion)
                    {
                        m_updateAvailable = true;
                    }

                    break;
                }

                if (latestApplicableVersion)
                {
                    // DefaultInstallVersion hasn't been created yet, create and populate it.
                    // DefaultInstallVersion is the latest applicable version of the internal package object.
                    auto latestVersionImpl = winrt::make_self<wil::details::module_count_wrapper<
                        winrt::Microsoft::Management::Deployment::implementation::PackageVersionInfo>>();
                    latestVersionImpl->Initialize(std::move(latestApplicableVersion));
                    m_latestApplicableVersion = *latestVersionImpl;
                }
            });
    }

    Microsoft::Management::Deployment::PackageVersionInfo CatalogPackage::DefaultInstallVersion()
    {
        InitializeLatestApplicableVersion();

        if (m_latestApplicableVersion)
        {
            return m_latestApplicableVersion;
        }
        else if (AvailableVersions().Size() > 0)
        {
            return GetPackageVersionInfo(AvailableVersions().GetAt(0));
        }
        else
        {
            return nullptr;
        }
    }

    Microsoft::Management::Deployment::PackageVersionInfo CatalogPackage::GetPackageVersionInfo(Microsoft::Management::Deployment::PackageVersionId const& versionKey)
    {
        winrt::Microsoft::Management::Deployment::PackageVersionInfo packageVersionInfo{ nullptr };

        ::AppInstaller::Repository::PackageVersionKey internalVersionKey(winrt::to_string(versionKey.PackageCatalogId()), winrt::to_string(versionKey.Version()), winrt::to_string(versionKey.Channel()));
        std::shared_ptr<::AppInstaller::Repository::IPackageVersion> availableVersion =
            ::AppInstaller::Repository::GetAllAvailableVersions(m_package)->GetVersion(internalVersionKey);
        if (availableVersion)
        {
            auto packageVersionInfoImpl = winrt::make_self<wil::details::module_count_wrapper<
                winrt::Microsoft::Management::Deployment::implementation::PackageVersionInfo>>();
            packageVersionInfoImpl->Initialize(std::move(availableVersion));
            packageVersionInfo =*packageVersionInfoImpl;
        }
        return packageVersionInfo;
    }

    bool CatalogPackage::IsUpdateAvailable()
    {
        InitializeLatestApplicableVersion();
        return m_updateAvailable;
    }

    Windows::Foundation::IAsyncOperation<winrt::Microsoft::Management::Deployment::CheckInstalledStatusResult> CatalogPackage::CheckInstalledStatusAsync(
        Microsoft::Management::Deployment::InstalledStatusType checkTypes)
    {
        co_return CheckInstalledStatus(checkTypes);
    }
    Microsoft::Management::Deployment::CheckInstalledStatusResult CatalogPackage::CheckInstalledStatus(
        Microsoft::Management::Deployment::InstalledStatusType checkTypes)
    {
        Microsoft::Management::Deployment::CheckInstalledStatusResultStatus status = winrt::Microsoft::Management::Deployment::CheckInstalledStatusResultStatus::Ok;
        Windows::Foundation::Collections::IVector<Microsoft::Management::Deployment::PackageInstallerInstalledStatus> installedStatus{
            winrt::single_threaded_vector<Microsoft::Management::Deployment::PackageInstallerInstalledStatus>() };

        try
        {
            auto checkResult = ::AppInstaller::Repository::CheckPackageInstalledStatus(m_package, static_cast<::AppInstaller::Repository::InstalledStatusType>(checkTypes));

            // Build the result object from the checkResult
            for (auto const& entry : checkResult)
            {
                auto checkInstallerResult = winrt::make_self<wil::details::module_count_wrapper<
                    winrt::Microsoft::Management::Deployment::implementation::PackageInstallerInstalledStatus>>();
                checkInstallerResult->Initialize(entry);

                installedStatus.Append(*checkInstallerResult);
            }
        }
        catch (...)
        {
            status = winrt::Microsoft::Management::Deployment::CheckInstalledStatusResultStatus::InternalError;
        }

        auto checkInstalledStatusResult = winrt::make_self<wil::details::module_count_wrapper<
            winrt::Microsoft::Management::Deployment::implementation::CheckInstalledStatusResult>>();
        checkInstalledStatusResult->Initialize(status, installedStatus);
        return *checkInstalledStatusResult;
    }
    winrt::Windows::Foundation::IAsyncOperation<winrt::Microsoft::Management::Deployment::CheckInstalledStatusResult> CatalogPackage::CheckInstalledStatusAsync()
    {
        co_return CheckInstalledStatus(InstalledStatusType::AllChecks);
    }
    winrt::Microsoft::Management::Deployment::CheckInstalledStatusResult CatalogPackage::CheckInstalledStatus()
    {
        return CheckInstalledStatus(InstalledStatusType::AllChecks);
    }
    std::shared_ptr<::AppInstaller::Repository::ICompositePackage> CatalogPackage::GetRepositoryPackage()
    {
        return m_package;
    }
}
