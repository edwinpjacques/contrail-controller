#
# Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
#

import setuptools


setuptools.setup(
    name='kube_manager',
    version='0.1dev',
    packages=setuptools.find_packages(),
    package_data={'': ['*.html', '*.css', '*.xml', '*.yml']},

    # metadata
    author="OpenContrail",
    author_email="dev@lists.opencontrail.org",
    license="Apache Software License",
    url="http://www.opencontrail.org/",

    long_description="Kubernetes Network Manager",

    test_suite='kube_manager.tests',

    install_requires=['future', 'six'],

    entry_points = {
        # Please update sandesh/common/vns.sandesh on process name change
        'console_scripts' : [
            'contrail-kube-manager = kube_manager.kube_manager:main',
        ],
    },
)
