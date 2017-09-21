// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationComponent.h"

#include "AICollision.h"
#include "Environment.h"

#include "Navigation/NavigationSystemSchematyc.h"

void CEntityAINavigationComponent::SMovementProperties::ReflectType(Schematyc::CTypeDesc<SMovementProperties>& desc)
{
	desc.SetGUID("44888426-aafa-472e-81ac-58bb1be18475"_cry_guid);

	desc.SetLabel("Movement Properties");
	desc.SetDescription("Movement Properties");
	desc.SetDefaultValue(SMovementProperties());

	desc.AddMember(&SMovementProperties::normalSpeed, 'nrsp', "normalSpeed", "Normal Speed", "Normal speed of the agent", 4.0f);
	desc.AddMember(&SMovementProperties::minSpeed, 'mnsp', "minSpeed", "Min Speed", "Minimal speed of the agent", 0.0f);
	desc.AddMember(&SMovementProperties::maxSpeed, 'mxsp', "maxSpeed", "Max Speed", "Maximal speed of the agent", 6.0f);

	desc.AddMember(&SMovementProperties::maxAcceleration, 'mxac', "maxAcceleration", "Max Acceleration", "Maximal acceleration", 6.0f);
	desc.AddMember(&SMovementProperties::maxDeceleration, 'mxdc', "maxDeceleration", "Max Deceleration", "Maximal deceleration", 10.0f);

	desc.AddMember(&SMovementProperties::lookAheadDistance, 'lad', "lookAheadDistance", "Look Ahead Distance", "How far is point on path that the agent is following", 0.5f);
	desc.AddMember(&SMovementProperties::bStopAtEnd, 'sae', "stopAtEnd", "Stop At End", "Should the agent stop when reaching end of the path", true);
}

void ReflectType(Schematyc::CTypeDesc<CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType>& desc)
{
	desc.SetGUID("ff0a97c4-eb3f-44cb-b23a-1626fc073e4c"_cry_guid);

	desc.AddConstant(CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::None, "None", "None");
	desc.AddConstant(CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::Passive, "Passive", "Passive");
	desc.AddConstant(CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::Active, "Active", "Active");
}

void CEntityAINavigationComponent::SCollisionAvoidanceProperties::ReflectType(Schematyc::CTypeDesc<SCollisionAvoidanceProperties>& desc)
{ 
	desc.SetGUID("4086c4b2-2300-41ea-aa95-4f119fa4281b"_cry_guid);

	desc.AddMember(&SCollisionAvoidanceProperties::type, 'type', "type", "Type", "How the agent is going to behave in collision avoidance system", SCollisionAvoidanceProperties::EType::Active);
	desc.AddMember(&SCollisionAvoidanceProperties::radius, 'rad', "radius", "Radius", "Radius of the agent used in collision avoidance calculations", 0.3f);
}

void CEntityAINavigationComponent::SStateUpdatedSignal::ReflectType(Schematyc::CTypeDesc<SStateUpdatedSignal>& desc)
{
	desc.SetGUID("cb398038-bf76-4c32-935a-0956dda8fc69"_cry_guid);
	desc.SetLabel("StateUpdated");
	desc.SetDescription("Sent when a navigation movement state is updated and new requested velocity is computed.");
	desc.AddMember(&SStateUpdatedSignal::m_requestedVelocity, 'vel', "requestedVel", "Requested Velocity", nullptr, ZERO);
}

void CEntityAINavigationComponent::SNavigationCompletedSignal::ReflectType(Schematyc::CTypeDesc<SNavigationCompletedSignal>& desc)
{
	desc.SetGUID("a6a897d3-2a30-4bc2-b14c-823baf2a2dc6"_cry_guid);
	desc.SetLabel("NavigationCompleted");
	desc.SetDescription("Sent when a navigation request is completed.");
}

void CEntityAINavigationComponent::SNavigationFailedSignal::ReflectType(Schematyc::CTypeDesc<SNavigationFailedSignal>& desc)
{
	desc.SetGUID("52d3b5de-7096-4de7-9ec7-a769e092d2d2"_cry_guid);
	desc.SetLabel("NavigationFailed");
	desc.SetDescription("Sent when a navigation request failed.");
}

CEntityAINavigationComponent::CEntityAINavigationComponent() 
	: m_movementAdapter(this)
	, m_collisionAgent(this)
	, m_pPathfollower(nullptr)
	, m_movementRequestId(0)
	, m_pathRequestId(0)
	, m_currentPosition(ZERO)
	, m_currentVelocity(ZERO)
	, m_requestedVelocity(ZERO)
	, m_lastPathRequestFailed(false)
{
}

CEntityAINavigationComponent::~CEntityAINavigationComponent()
{
}

void CEntityAINavigationComponent::ReflectType(Schematyc::CTypeDesc<CEntityAINavigationComponent>& desc)
{
	desc.SetGUID(CEntityAINavigationComponent::IID());

	desc.SetLabel("AI Navigation Agent");
	desc.SetDescription("Navigation system agent component");
	desc.SetIcon("icons:Navigation/Move_Classic.ico");
	desc.SetEditorCategory("AI");
	desc.SetIcon("icons:Navigation/Move_Classic.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Singleton, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

	desc.AddMember(&CEntityAINavigationComponent::m_agentTypeId, 'agt', "agent", "Navigation Agent Type", nullptr, NavigationAgentTypeID());
	desc.AddMember(&CEntityAINavigationComponent::m_bUpdateTransformation, 'ut', "updateTransf", "Update Transformation", "Entity transformation is automatically modified by the agent when enabled", false);
	desc.AddMember(&CEntityAINavigationComponent::m_movementProperties, 'move', "movement", "Movement", nullptr, SMovementProperties());
	desc.AddMember(&CEntityAINavigationComponent::m_collisionAvoidanceProperties, 'cola', "colavoid", "Collision Avoidance", nullptr, SCollisionAvoidanceProperties());
}

void CEntityAINavigationComponent::Register(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CEntityAINavigationComponent));

		// Functions
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAINavigationComponent::SetRequestedVelocity, "e99ce354-1c57-404f-926e-b56b0254a12d"_cry_guid, "SetRequestedVelocity");
			pFunction->SetDescription("Sets requested movement velocity");
			pFunction->BindInput(1, 'vel', "Velocity", "Requested movement velocity", Vec3(ZERO));
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAINavigationComponent::GetRequestedVelocity, "d6e2454f-fb39-4c7e-bec4-615e33c23322"_cry_guid, "GetRequestedVelocity");
			pFunction->SetDescription("Returns requested movement velocity");
			pFunction->BindOutput(0, 'vel', "Velocity", "Requested movement velocity");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAINavigationComponent::NavigateTo, "d7ef4d30-dc99-41df-a0b5-eb21760b84a3"_cry_guid, "NavigateTo");
			pFunction->SetDescription("Finds path to destination and starts navigating to it");
			pFunction->BindInput(1, 'pos', "Destination", "Destination position", Vec3(ZERO));
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAINavigationComponent::IsDestinationReachable, "86b5dd4d-518d-4baa-b3e6-c3bae725bb88"_cry_guid, "IsDestinationReachable");
			pFunction->SetDescription("Check if there is possible path to destination point for this entity.");
			pFunction->BindInput(1, 'pos', "Destination", "Destination position", Vec3(ZERO));
			pFunction->BindOutput(0, 'ir', "IsReachable", "Is Reachable");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAINavigationComponent::StopMovement, "c4c3fc49-5884-48c6-9d71-bbd30ad7dced"_cry_guid, "Stop");
			pFunction->SetDescription("Stops current navigation");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAINavigationComponent::TestRaycastHit, "3b354256-7197-4fce-9d37-4ff37bc07c76"_cry_guid, "TestRaycastHit");
			pFunction->SetDescription("Returns true when raycast on navmesh hits its boundaries");
			pFunction->BindInput(1, 'pos', "ToPositon", "Destination position", Vec3(ZERO));
			pFunction->BindOutput(0, 'hit', "IsHit", "Raycast Hit");
			pFunction->BindOutput(2, 'hp', "HitPosition", "Hit Position");
			pFunction->BindOutput(3, 'hn', "HitNormal", "Hit Normal");
			componentScope.Register(pFunction);
		}
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAINavigationComponent::IsRayObstructed, "3813108c-e614-46ec-a029-f5ab0b85500c"_cry_guid, "IsRayObstructed");
			pFunction->SetDescription("Returns true when ray is obstructed. Use this function instead of Raycast when you are not interested in hit data.");
			pFunction->BindInput(1, 'pos', "ToPositon", "Destination position", Vec3(ZERO));
			pFunction->BindOutput(0, 'hit', "IsObstructed", "True if obstructed");
			componentScope.Register(pFunction);
		}

		// Signals
		{
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SStateUpdatedSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SNavigationCompletedSignal));
			componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SNavigationFailedSignal));
		}
	}
}
	
void CEntityAINavigationComponent::Initialize()
{
	PathFollowerParams pathFollowerParams;
	FillPathFollowerParams(pathFollowerParams);

	m_pPathfollower = gEnv->pAISystem->CreateAndReturnNewDefaultPathFollower(pathFollowerParams, m_pathObstacles);
	m_movementAdapter.SetPathFollower(m_pPathfollower.get());

	Reset();
}

void CEntityAINavigationComponent::OnShutDown()
{
	Stop();

	m_pPathfollower = nullptr;
	m_movementAdapter.SetPathFollower(nullptr);
}

void CEntityAINavigationComponent::Reset()
{
	m_currentVelocity = ZERO;
	m_currentPosition = GetEntity()->GetWorldPos();

	m_movementAdapter.SetNavigationAgentTypeID(m_agentTypeId);
}

void CEntityAINavigationComponent::Start()
{
	CRY_ASSERT(gEnv && gEnv->pAISystem && gEnv->pAISystem->GetMovementSystem());
	
	Reset();

	MovementActorCallbacks callbacks;
	callbacks.queuePathRequestFunction = functor(*this, &CEntityAINavigationComponent::QueueRequestPathFindingRequest);
	callbacks.checkOnPathfinderStateFunction = functor(*this, &CEntityAINavigationComponent::CheckOnPathfinderState);
	callbacks.getPathFunction = functor(*this, &CEntityAINavigationComponent::GetNavPath);
	callbacks.getPathFollowerFunction = functor(*this, &CEntityAINavigationComponent::GetPathfollower);

	gEnv->pAISystem->GetMovementSystem()->RegisterEntity(GetEntity()->GetId(), callbacks, m_movementAdapter);
	m_collisionAgent.Initialize((m_collisionAvoidanceProperties.type == SCollisionAvoidanceProperties::EType::None) ? nullptr : GetEntity());
}

void CEntityAINavigationComponent::Stop()
{
	CRY_ASSERT(gEnv && gEnv->pAISystem && gEnv->pAISystem->GetMovementSystem());

	Reset();
	
	CancelRequestedPath();
	CancelCurrentMovementRequest();
	
	gEnv->pAISystem->GetMovementSystem()->UnregisterEntity(GetEntity()->GetId());
	m_collisionAgent.Release();

	m_pNavigationPath = nullptr;
}

bool CEntityAINavigationComponent::IsGameOrSimulation() const
{
	return gEnv->IsGameOrSimulation();
}

uint64 CEntityAINavigationComponent::GetEventMask() const 
{ 
	uint64 eventMask = kDefaultEntityEventMask;

	IEntity* pEntity = GetEntity();
	if (IsGameOrSimulation())
	{
		if (m_bUpdateTransformation)
		{
			eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
		}
		if (!pEntity->GetPhysics())
		{
			eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_PREPHYSICSUPDATE);
		}
	}
	return eventMask;
}

void CEntityAINavigationComponent::ProcessEvent(SEntityEvent& event)
{
	switch (event.event)
	{
		case EEntityEvent::ENTITY_EVENT_UPDATE:
		{
			SEntityUpdateContext* updateContext = reinterpret_cast<SEntityUpdateContext*>(event.nParam[0]);
			UpdateTransformation(updateContext->fFrameTime);
			break;
		}
		case EEntityEvent::ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			UpdateVelocity(event.fParam[0]);
			break;
		}
		case EEntityEvent::ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
		{
			if (IsGameOrSimulation())
			{
				m_collisionAgent.Initialize((m_collisionAvoidanceProperties.type == SCollisionAvoidanceProperties::EType::None) ? nullptr : GetEntity());
			}
			GetEntity()->UpdateComponentEventMask(this);
			break;
		}
		case EEntityEvent::ENTITY_EVENT_RESET:
		{
			if (GetEntity()->GetSimulationMode() != EEntitySimulationMode::Game)
			{
				Stop();
				GetEntity()->UpdateComponentEventMask(this);
			}
			break;
		}
		case EEntityEvent::ENTITY_EVENT_LEVEL_LOADED:
		{
			if (IsGameOrSimulation())
			{
				Start();
				GetEntity()->UpdateComponentEventMask(this);
			}
			break;
		}
	}
}

void CEntityAINavigationComponent::UpdateVelocity(float deltaTime)
{
	if (!GetEntity()->GetPhysics())
	{
		if (deltaTime > 0.0001f)
		{
			const Vec3 currentPos = GetEntity()->GetWorldPos();
			
			// Update current velocity
			m_currentVelocity = (currentPos - m_currentPosition) / deltaTime;
			m_currentPosition = currentPos;
		}
	}
}

void CEntityAINavigationComponent::UpdateTransformation(float deltaTime)
{
	IEntity* pEntity = GetEntity();

	const float lenSquared2D = m_requestedVelocity.GetLengthSquared();

	if (IPhysicalEntity* pPhysics = pEntity->GetPhysics())
	{
		// Update with physics
		if (lenSquared2D >= sqr(0.001f))
		{
			// Update rotation
			const Vec3 velocityNormalized = m_requestedVelocity.GetNormalized();
			Matrix33 rotationMatrix;
			rotationMatrix.SetRotationVDir(velocityNormalized);
			
			pe_params_pos ppos;
			ppos.q = Quat(rotationMatrix);
			pPhysics->SetParams(&ppos);
		}

		// Move
		const pe_type physicType = pPhysics->GetType();
		if (physicType == PE_LIVING)
		{
			pe_action_move move;
			move.dir = m_requestedVelocity;
			move.dt = deltaTime;
			move.iJump = 0;
			pPhysics->Action(&move);
		}

		if (!m_requestedVelocity.IsZero() && physicType != PE_ARTICULATED)
		{
			pe_action_set_velocity action_set_velocity;
			action_set_velocity.v = m_requestedVelocity;
			pPhysics->Action(&action_set_velocity);
		}
	}
	else if (!m_requestedVelocity.IsZero())
	{
		//No physics
		const Vec3 currentScale = pEntity->GetScale();
		Matrix33 scaleMatrix;
		scaleMatrix.SetScale(currentScale);

		Matrix34 localTM = pEntity->GetLocalTM();

		const Vec3 newPosition = localTM.GetTranslation() + (m_requestedVelocity * deltaTime);
		Vec3 adjustedPosition = newPosition;
		FindFloor(newPosition, adjustedPosition);

		localTM.SetTranslation(adjustedPosition);

		if (lenSquared2D >= sqr(0.001f))
		{
			// Update rotation
			const Vec3 velocityNormalized = m_requestedVelocity.GetNormalized();
			Matrix33 rotationMatrix;
			rotationMatrix.SetRotationVDir(velocityNormalized);

			localTM.SetRotation33(rotationMatrix * scaleMatrix);
		}		
		pEntity->SetLocalTM(localTM);
	}
	m_requestedVelocity.zero();
}

bool CEntityAINavigationComponent::IsRayObstructed(const Vec3& toPosition) const
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();
	IMNMPathfinder* pPathfinder = gEnv->pAISystem->GetMNMPathfinder();

	const Vec3& startPosition = GetPosition();
	
	const NavigationMeshID startMeshId = pNavigationSystem->GetEnclosingMeshID(m_agentTypeId, startPosition);
	if (!startMeshId)
		return true;

	const NavigationMeshID endMeshId = pNavigationSystem->GetEnclosingMeshID(m_agentTypeId, toPosition);
	if (!startMeshId)
		return true;

	if (startMeshId != endMeshId)
		return true;

	return !pPathfinder->CheckIfPointsAreOnStraightWalkableLine(startMeshId, startPosition, toPosition, 0.0f);
}

bool CEntityAINavigationComponent::TestRaycastHit(const Vec3& toPositon, Vec3& hitPos, Vec3& hitNorm) const
{
	MNM::SRayHitOutput rayHit;
	const Vec3& startPos = GetPosition();

	if (gEnv->pAISystem->GetNavigationSystem()->NavMeshTestRaycastHit(m_agentTypeId, startPos, toPositon, &rayHit))
	{
		hitPos = rayHit.position;
		hitNorm = rayHit.normal2D;
		return true;
	}
	return false;
}

bool CEntityAINavigationComponent::IsDestinationReachable(const Vec3& destination) const
{
	return gEnv->pAISystem->GetNavigationSystem()->IsPointReachableFromPosition(m_agentTypeId, GetEntity(), GetPosition(), destination);
}

void CEntityAINavigationComponent::StopMovement()
{
	CancelCurrentMovementRequest();

	MovementRequest request;
	request.type = MovementRequest::Stop;
	request.entityID = GetEntity()->GetId();
	request.callback = functor(*this, &CEntityAINavigationComponent::MovementRequestCompleted);

	QueueMovementRequest(request);
}

void CEntityAINavigationComponent::NavigateTo(const Vec3& targetPosition)
{
	CancelCurrentMovementRequest();
		
	MovementRequest request;
	request.destination = targetPosition;
	request.type = MovementRequest::MoveTo;
	request.entityID = GetEntity()->GetId();
	request.callback = functor(*this, &CEntityAINavigationComponent::MovementRequestCompleted);

	/* TODO: these MovementRequest params aren't set now and have default values
	MovementStyle   style;
	MNMDangersFlags dangersFlags;
	bool            considerActorsAsPathObstacles;
	float           lengthToTrimFromThePathEnd;
	*/

	QueueMovementRequest(request);
}

Vec3 CEntityAINavigationComponent::GetRequestedVelocity() const
{
	return m_requestedVelocity;
}

void CEntityAINavigationComponent::SetRequestedVelocity(const Vec3& velocity)
{
	m_requestedVelocity = velocity;
}

bool CEntityAINavigationComponent::QueueMovementRequest(const MovementRequest& request)
{
	CRY_ASSERT(gEnv && gEnv->pAISystem && gEnv->pAISystem->GetMovementSystem());
	m_movementRequestId = gEnv->pAISystem->GetMovementSystem()->QueueRequest(request);
	return true;
}

void CEntityAINavigationComponent::CancelCurrentMovementRequest()
{
	CRY_ASSERT(gEnv && gEnv->pAISystem && gEnv->pAISystem->GetMovementSystem());

	if (m_movementRequestId != MovementRequestID::Invalid())
	{
		gEnv->pAISystem->GetMovementSystem()->CancelRequest(m_movementRequestId);
		m_movementRequestId = MovementRequestID::Invalid();
	}
}

void CEntityAINavigationComponent::MovementRequestCompleted(const MovementRequestResult& result)
{
	if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
	{
		if (result.result == MovementRequestResult::Success)
		{
			pSchematycObject->ProcessSignal(SNavigationCompletedSignal(), GetGUID());
		}
		else
		{
			pSchematycObject->ProcessSignal(SNavigationFailedSignal(), GetGUID());
		}
	}
}

void CEntityAINavigationComponent::QueueRequestPathFindingRequest(MNMPathRequest& request)
{
	m_lastPathRequestFailed = false;
	request.resultCallback = functor(*this, &CEntityAINavigationComponent::OnMNMPathfinderResult);
	m_pathRequestId = gEnv->pAISystem->GetMNMPathfinder()->RequestPathTo(GetEntity()->GetId(), request);
}

void CEntityAINavigationComponent::CancelRequestedPath()
{
	if (m_pathRequestId)
	{
		gEnv->pAISystem->GetMNMPathfinder()->CancelPathRequest(m_pathRequestId);
		m_pathRequestId = 0;
	}
}

void CEntityAINavigationComponent::OnMNMPathfinderResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result)
{
	m_pathRequestId = 0;
	if (result.HasPathBeenFound())
	{
		m_pNavigationPath = result.pPath;
		m_pPathfollower->AttachToPath(result.pPath.get());
	}
	else
	{
		m_lastPathRequestFailed = true;
	}
}

Movement::PathfinderState CEntityAINavigationComponent::CheckOnPathfinderState()
{
	if (m_pathRequestId != 0)
	{
		return Movement::StillFinding;
	}
	if (!m_lastPathRequestFailed && m_pNavigationPath != nullptr)
	{
		return Movement::FoundPath;
	}
	return Movement::CouldNotFindPath;
}

void CEntityAINavigationComponent::NewStateComputed(const PathFollowResult& result, void* pSender)
{
	m_requestedVelocity = result.velocityOut;

	if (m_collisionAvoidanceProperties.type != SCollisionAvoidanceProperties::EType::Active || pSender == &m_collisionAgent)
	{
		if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
		{
			pSchematycObject->ProcessSignal(SStateUpdatedSignal{ result.velocityOut }, GetGUID());
		}

		if (m_stateUpdatedCallback)
		{
			m_stateUpdatedCallback(result.velocityOut);
		}
	}
}

void CEntityAINavigationComponent::FillPathFollowerParams(PathFollowerParams& params) const
{
	params.use2D = true;
	params.snapEndPointToGround = true;
	
	params.minSpeed = m_movementProperties.minSpeed;
	params.maxSpeed = m_movementProperties.maxSpeed;
	params.normalSpeed = crymath::clamp(m_movementProperties.normalSpeed, m_movementProperties.minSpeed, m_movementProperties.maxSpeed);
	params.endDistance = 0.0f;
	params.maxAccel = m_movementProperties.maxAcceleration;
	params.maxDecel = m_movementProperties.maxDeceleration;
	params.stopAtEnd = m_movementProperties.bStopAtEnd;
}

Vec3 CEntityAINavigationComponent::GetPosition() const
{
	return GetEntity()->GetWorldPos();
}

Vec3 CEntityAINavigationComponent::GetVelocity() const
{
	Vec3 velocity = m_currentVelocity;
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		// If living entity return that vel since that is actually the rate of change of position
		pe_status_living pstatus;
		if (pPhysicalEntity->GetStatus(&pstatus))
		{
			velocity = pstatus.vel;
		}
		else
		{
			pe_status_dynamics dSt;
			pPhysicalEntity->GetStatus(&dSt);
			velocity = dSt.v;
		}
	}
	return velocity;
}

CRY_STATIC_AUTO_REGISTER_FUNCTION(&CEntityAINavigationComponent::Register)