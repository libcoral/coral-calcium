
function companyUpdate( node )
	local company = node.company
	
	local projects = company.projects
	local serviceList = {}
	local productList = {}
	
	for _, project in ipairs( projects ) do
		update( project._providerTable, spaceLoader )
		local projectUpdated = project._providerTable
		if( project.isService ) then
			serviceList[ #serviceList + 1 ] = projectUpdated.service
		else
			productList[ #productList + 1 ] = projectUpdated.product
		end
	end
	company.services = serviceList
	company.products = productList
	company.projects = nil
end

function projectUpdate( node )
	local devs = {}
	
	local project = node.project
	
	for _, dev in ipairs( project.developers ) do
		devProvider = dev._providerTable
		update( devProvider )
		devs[ #devs + 1 ] = devProvider.employee
	end
	if ( project.isService ) then

		node._type = "dom.Service"
		
		node.service = {
						_type = "dom.IService", 
						name = project.name,
						monthlyIncome = project.earnings,
						mantainers = devs,
						_providerTable = node,
					}
		
		node.project = nil
	else
		node._type = "dom.Product"
		
		node.product = {
						_type = "dom.IProduct",
						name = project.name,
						value = project.earnings,
						developers = devs,
						_providerTable = node,
					}
		leaderObj = project.manager._providerTable
		update( leaderObj )
		node.product.leader = leaderObj.employee
		node.project = nil

	end
	
end

function convertToEmployee( node )
	if( node._type == "dom.Manager" ) then
		node.employee = node.manager
		node.employee.role = "Manager"
		node.manager = nil
	else
		node.employee = node.developer
		node.employee.role = "Developer"
		node.developer = nil
	end
	node.employee._type = "dom.IEmployee"
	node._type = "dom.Employee"
		
end

local updateFunctions
updateFunctions = { ["dom.Company"] = companyUpdate, ["dom.Manager"] = convertToEmployee, ["dom.Developer"] = convertToEmployee, ["dom.Project"] = projectUpdate,}
		
function update( node )
	
	if node ~= nil then
		local updateFunc = updateFunctions[ node._type ]
		
		updateFunc( node )
	end
	
end
