---
title: export Command
description: exports the list of installed applications.
ms.date: 05/02/2021
ms.topic: overview
ms.localizationpriority: medium
---

# export command (winget)

The **export** command of the [winget](index.md) tool exports a JSON file of apps to a specified file.  The **export** command users JSON as the format.  See [the JSON schema used by **winget**](https://aka.ms/winget-packages.schema.1.0.json).

The **export** combined with the [**import**](import.md) command allows you to batch install applications on your PC.

The **export** command is often used to create a file that you can share with other developers, or for use when restoring your build environment.

## Usage

`winget export [-o] <output> [<options>]`

![export](images/export.png)

## Arguments

The following arguments are available.
| Argument    | Description |
|-------------|-------------|
| **-o,--output** | Path to the JSON file to be created

## Options

The options allow you to customize the export experience to meet your needs.

| Option | Description |
|--------|-------------|
| **-s, --source**  |  [optional] Specifies a source to export files from.  Use this option when you only want files from a specific source.  |
| **--include-versions** | [optional] Includes the version of the app currently installed.  Use this option if you want a specific version.  By default, unless specified, [**import**](import.md) will use latest. |
| **--accept-source-agreements**  | Accept all source agreements during source operations |
| **-?, --help** | Shows help about the selected command |
| **--wait** | Prompts the user to press any key before exiting |
| **--logs, --open-logs** | Open the default logs location |
| **--verbose, --verbose-logs** | Enables verbose logging for winget |
| **--disable-interactivity** | Disable interactive prompts |

## JSON Schema
The driving force behind the **export** command is the JSON file.  As mentioned, you can see the [schema for the JSON file](https://aka.ms/winget-packages.schema.1.0.json).

The JSON file includes the following hierarchy:
| Entry | Description |
|-------------|-------------|
| **Sources**  |  The sources application manifests come from.  |
| **Packages**  |  The collection of packages to install.  |
| **PackageIdentifier**  |  The Windows Package Manager package identifier used to specify the package.  |
| **Version**  |  [Optional] The specific version of the package to install.  |

## exporting files

When the Windows Package Manager exports the JSON file, it attempts to export all the applications installed on the PC. If the **winget export** command is not able to match an application to an application from an available **source**, the export command will show a warning.

Note: matching an application depends on metadata in the manifest from a configured source, and metadata in Add / Remove Programs in Windows based on the package installer.

In the example below, you will see warnings for **WhatsApp Desktop** and **7-Zip**.

![export](images/export-command.png)

Once the export is complete, you can edit the resulting JSON file in your favorite editor. You can remove apps you do not wish to import in the future.

## Related topics

* [Use the winget tool to install and manage applications](index.md)

