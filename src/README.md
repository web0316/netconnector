# Application netconnector

The application `netconnector` runs either as the NetConnector service (the
'listener') or as a utility for managing the service.

As listener, `netconnector` implements the NetConnector interface described in
[netconnector.fidl](https://fuchsia.googlesource.com/netconnector/+/master/services/netconnector.fidl). Clients ('requestors') that want to initiate communication with a
service on a remote machine call `GetDeviceServiceProvider` and then
`ConnectToService` on the service provider. Apps that want to respond
need to be registered with `netconnector`, typically via its config file.

`netconnector` has command line options, and it will read a config file. It uses
the config file regardless of whether it runs as listener or as a utility.
Most of the command line options are also relevant to `netconnector` in both
modes.

The command line options for `netconnector` are:

    --no-config                 don't read a config file
    --config=<path>             use <path> rather than the default config file
    --service=<name>@<app url>  register a responding service
    --device=<name>@<ip>        register a device
    --listen                    run as listener

The `--listen` option makes `netconnector` run as listener. Typically, this
argument is only used in the context of `bootstrap`'s `services.config` file.
This is because applications started from the command line are always different
instances from applications started by `bootstrap`. In other words, you can
start a listener from the command line, but it will only conflict with the
instance that requestors will get when they connect to the service.

The options `--no-config` and `--config=<path>` control which config file
`netconnector` reads, if any. The default config file is located at
`/system/data/netconnector/netconnector.config`.

The rest of the options are used to specify the name of the local device and to
register responding services and remote devices. The config file provides the
same information. A config file looks like this:

    {
      "services": {
        "netconnector::Example": "file:///system/apps/netconnector_example"
      },
      "devices": {
        "acer": "192.168.4.118",
        "nuc": "192.168.4.60"
      }
    }

Command line options override or supplement the config file.

As mentioned previously, the `listen` option should generally only be used in
`bootstrap`'s `services.config` file. For example:

    {
      "services": {
        ...
        "netconnector::NetConnector": [
            "file:///system/apps/netconnector", "--listen"
        ],
        ...
      }
    }

This registers `netconnector` under the default interface name for the
`NetConnector` interface. The first time something attempts to connect to
that service, `netconnector` will start up, read its config file and run
indefinitely as listener. Note that `bootstrap` won't start `netconnector`
unless something tries to connect to it. Typically, that something would be
`netconnector` running in its utility mode.

Provided the listener is registered with `bootstrap` as shown above, the
listener can be started like this:

    $ netconnector

Requestors should run in the same context or a child context so they have
access to the listener.

Service and device registrations can be added to a running listener using the
utility. The utility will read the config
file unless told not to, so any new registrations that appear there will be
sent to the running listener. Currently, there is no support in the utility for
unregistering services or devices or for stopping the listener. The listener
can be stopped by killing its process.
