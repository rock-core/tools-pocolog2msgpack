require 'autoproj/git_server_configuration'
Autoproj.env_inherit 'CMAKE_PREFIX_PATH'

# don't check out history if possible
Autobuild::Git.shallow=true
Autobuild::Git.single_branch=true
